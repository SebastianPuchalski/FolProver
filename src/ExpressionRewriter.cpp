#include "ExpressionRewriter.hpp"

#include "ExpressionUtils.hpp"

#include <unordered_set>

using namespace ExpressionUtils;

namespace ExpressionRewriter {

bool isReplacementRuleCorrectRec(const DSL::F& f,
    std::unordered_set<std::string>& usedMetavarSymbols,
    std::unordered_set<std::string>& usedVarSymbols, bool pattern);
FormulaPtr rewriteRec(const FormulaPtr& formula,
    const std::vector<ReplacementRule>& rules, bool& anyChange);
FormulaPtr rewriteFastRec(const FormulaPtr& formula,
    const std::vector<ReplacementRule>& rules,
    std::unordered_set<ExpressionPtr>& checkedNodes);
bool matchesRec(const FormulaPtr& formula, const FormulaPtr& pattern,
    std::map<std::string, ExpressionPtr>& nameToExpr);
FormulaPtr applySubstitutionRec(const FormulaPtr& formula,
    const std::map<std::string, ExpressionPtr>& nameToExpr,
    std::map<std::string, int>& nameUsageCounts);

//------------------------------------------------------------------------------

namespace DSL {
    F True() { return std::make_shared<BooleanFormula>(true); }
    F False() { return std::make_shared<BooleanFormula>(false); }

    F Not(F f) { return std::make_shared<NegationFormula>(f); }

    F And(F l, F r) { return std::make_shared<BinaryFormula>(BinaryFormula::Operator::AND, l, r); }
    F Or(F l, F r) { return std::make_shared<BinaryFormula>(BinaryFormula::Operator::OR, l, r); }
    F Imp(F l, F r) { return std::make_shared<BinaryFormula>(BinaryFormula::Operator::IMP, l, r); }
    F Eqv(F l, F r) { return std::make_shared<BinaryFormula>(BinaryFormula::Operator::EQV, l, r); }
    F Xor(F l, F r) { return std::make_shared<BinaryFormula>(BinaryFormula::Operator::XOR, l, r); }

    F Forall(V variable, F body) {
        return std::make_shared<QuantificationFormula>(
            QuantificationFormula::Quantifier::FORALL,
            variable, body
        );
    }
    F Exists(V variable, F body) {
        return std::make_shared<QuantificationFormula>(
            QuantificationFormula::Quantifier::EXISTS,
            variable, body
        );
    }

    V Variable(const std::string& name) { return std::make_shared<VariableTerm>("V_" + name); }
    F Metavariable(const std::string& name) { return std::make_shared<PredicateFormula>("M_" + name); }

    Condition AreAlphaEquivalent(F metavariable1, F metavariable2) {
        Condition condition;
        condition.usedExprs = { metavariable1, metavariable2 };
        assert(metavariable1->exprType == Expression::Type::PREDICATE);
        assert(metavariable2->exprType == Expression::Type::PREDICATE);
        std::string name1 = std::static_pointer_cast<PredicateFormula>(metavariable1)->symbol;
        std::string name2 = std::static_pointer_cast<PredicateFormula>(metavariable2)->symbol;
        condition.check = [name1, name2](const std::map<std::string, ExpressionPtr>& map) -> bool {
            auto it1 = map.find(name1);
            auto it2 = map.find(name2);
            if (it1 == map.end() || it2 == map.end()) {
                assert(!"Metavariables must be present in the pattern");
                return false;
            }
            return areAlphaEquivalent(it1->second, it2->second);
            };

        return condition;
    }

    Condition NotFreeIn(V variable, F metavariable) {
        Condition condition;
        condition.usedExprs = { variable, metavariable };
        std::string varName = variable->symbol;
        assert(metavariable->exprType == Expression::Type::PREDICATE);
        std::string metavarName = std::static_pointer_cast<PredicateFormula>(metavariable)->symbol;
        condition.check = [varName, metavarName](const std::map<std::string, ExpressionPtr>& map) -> bool {
            auto varIt = map.find(varName);
            auto metavarIt = map.find(metavarName);
            if (varIt == map.end() || metavarIt == map.end()) {
                assert(!"Variable or metavariable must be present in the pattern");
                return false;
            }
            assert(varIt->second->exprType == Expression::Type::VARIABLE);
            auto variableTerm = std::static_pointer_cast<VariableTerm>(varIt->second);
            auto variableSymbol = variableTerm->symbol;
            auto expr = metavarIt->second;
            return !isVarFreeInExpr(expr, variableSymbol);
            };
        return condition;
    }
} // namespace DSL

bool isReplacementRuleCorrect(const ReplacementRule& rule) {
    if (!isDag(rule.pattern) || !isFullyDefined(rule.pattern)) return false;
    if (!isDag(rule.replacement) || !isFullyDefined(rule.replacement)) return false;

    std::unordered_set<std::string> usedMetavarSymbols;
    std::unordered_set<std::string> usedVarSymbols;

    if (!isReplacementRuleCorrectRec(
        rule.pattern, usedMetavarSymbols, usedVarSymbols, true)) return false;
    if (!isReplacementRuleCorrectRec(
        rule.replacement, usedMetavarSymbols, usedVarSymbols, false)) return false;

    for (const auto& cond : rule.conditions) {
        for (const auto& expr : cond.usedExprs) {
            if (expr->exprType == Expression::Type::PREDICATE) { // metavariable
                auto pred = std::static_pointer_cast<PredicateFormula>(expr);
                if (!usedMetavarSymbols.count(pred->symbol)) return false;
            }
            else if (expr->exprType == Expression::Type::VARIABLE) {
                auto var = std::static_pointer_cast<VariableTerm>(expr);
                if (!usedVarSymbols.count(var->symbol)) return false;
            }
            else return false;
        }
    }
    return true;
}

bool areReplacementRulesCorrect(
    const std::vector<ReplacementRule>& rules) {
    for (const auto& rule : rules) {
        if (!isReplacementRuleCorrect(rule)) return false;
    }
    return true;
}

FormulaPtr rewrite(const FormulaPtr& formula,
    const std::vector<ReplacementRule>& rules, bool inPlace) {
    assert(isTree(formula) && isFullyDefined(formula));
    assert(isArityConsistent(formula));
    assert(areReplacementRulesCorrect(rules));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    bool anyChange = true;
    while (anyChange) {
        anyChange = false;
        f = rewriteRec(f, rules, anyChange);
    }
    return f;
}

FormulaPtr rewriteFast(const FormulaPtr& formula,
    const std::vector<ReplacementRule>& rules, bool inPlace) {
    assert(isTree(formula) && isFullyDefined(formula));
    assert(isArityConsistent(formula));
    assert(areReplacementRulesCorrect(rules));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    std::unordered_set<ExpressionPtr> checkedNodes;
    return rewriteFastRec(f, rules, checkedNodes);
}

bool isReplacementRuleCorrectRec(const DSL::F& f,
    std::unordered_set<std::string>& usedMetavarSymbols,
    std::unordered_set<std::string>& usedVarSymbols, bool pattern) {

    if (!f || f->isTerm() ||
        f->exprType == Expression::Type::JUNCTION ||
        f->exprType == Expression::Type::EQUALITY) return false;

    if (f->exprType == Expression::Type::PREDICATE) {
        auto pred = std::static_pointer_cast<PredicateFormula>(f);
        const auto& symbol = pred->symbol;
        if (pred->getChildCount()) return false;
        if (pattern) {
            if (usedMetavarSymbols.count(symbol)) return false;
            usedMetavarSymbols.insert(symbol);
        }
        else {
            if (!usedMetavarSymbols.count(symbol)) return false;
        }
    }
    else if (f->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(f);
        const auto& variable = quant->variable;
        if (!variable || variable->symbol.empty()) return false;
        if (pattern) {
            usedVarSymbols.insert(variable->symbol);
        }
        else {
            if (!usedVarSymbols.count(variable->symbol)) return false;
        }
    }

    size_t count = f->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        FormulaPtr child = std::static_pointer_cast<Formula>(f->getChild(i));
        if (!isReplacementRuleCorrectRec(
            child, usedMetavarSymbols, usedVarSymbols, pattern)) return false;
    }
    return true;
}

FormulaPtr rewriteRec(const FormulaPtr& formula,
    const std::vector<ReplacementRule>& rules, bool& anyChange) {
    assert(formula && "Formula pointer is null");
    if (!formula) return nullptr;
    FormulaPtr current = formula;

    std::map<std::string, ExpressionPtr> nameToExpr;
    std::map<std::string, int> nameUsageCounts;
    size_t i = 0;
    while (i < rules.size()) {
        const auto& rule = rules[i++];
        nameToExpr.clear();
        if (matchesRec(current, rule.pattern, nameToExpr)) {
            bool conditionsMet = true;
            for (const auto& cond : rule.conditions) {
                if (!cond.check(nameToExpr)) {
                    conditionsMet = false;
                    break;
                }
            }
            if (conditionsMet) {
                anyChange = true;
                auto newFormula = std::static_pointer_cast<Formula>(rule.replacement->clone());
                nameUsageCounts.clear();
                current = applySubstitutionRec(newFormula, nameToExpr, nameUsageCounts);
                i = 0; // start over
            }
        }
    }

    if (current->isAtom()) return current;

    size_t count = current->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        FormulaPtr child = std::static_pointer_cast<Formula>(current->getChild(i));
        FormulaPtr newChild = rewriteRec(child, rules, anyChange);
        if (child != newChild) {
            current->setChild(i, newChild);
        }
    }
    return current;
}

FormulaPtr rewriteFastRec(const FormulaPtr& formula,
    const std::vector<ReplacementRule>& rules,
    std::unordered_set<ExpressionPtr>& checkedNodes) {
    assert(formula && "Formula pointer is null");
    if (!formula) return nullptr;
    FormulaPtr current = formula;

    if (!current->isAtom()) {
        size_t count = current->getChildCount();
        for (size_t i = 0; i < count; ++i) {
            auto child = std::static_pointer_cast<Formula>(current->getChild(i));
            if (!checkedNodes.count(child)) {
                auto newChild = rewriteFastRec(child, rules, checkedNodes);
                if (child != newChild) {
                    current->setChild(i, newChild);
                }
            }
        }
    }

    std::map<std::string, ExpressionPtr> nameToExpr;
    for (const auto& rule : rules) {
        nameToExpr.clear();
        if (matchesRec(current, rule.pattern, nameToExpr)) {
            bool conditionsMet = true;
            for (const auto& cond : rule.conditions) {
                if (!cond.check(nameToExpr)) {
                    conditionsMet = false;
                    break;
                }
            }
            if (conditionsMet) {
                auto newFormula = std::static_pointer_cast<Formula>(rule.replacement->clone());
                std::map<std::string, int> nameUsageCounts;
                auto transformed = applySubstitutionRec(newFormula, nameToExpr, nameUsageCounts);
                return rewriteFastRec(transformed, rules, checkedNodes);
            }
        }
    }

    checkedNodes.insert(current);
    return current;
}

bool matchesRec(const FormulaPtr& formula, const FormulaPtr& pattern,
    std::map<std::string, ExpressionPtr>& nameToExpr) {
    assert(formula && pattern && "Formula pointer is null");
    if (!formula || !pattern) return false;
    if (pattern->exprType == Expression::Type::JUNCTION ||
        pattern->exprType == Expression::Type::EQUALITY) {
        assert(!"Junction and equality is invalid in pattern");
        return false;
    }

    if (pattern->exprType == Expression::Type::PREDICATE) {
        auto name = std::static_pointer_cast<PredicateFormula>(pattern)->symbol;
        auto it = nameToExpr.find(name);
        if (it == nameToExpr.end()) {
            nameToExpr[name] = formula;
        }
        else {
            assert(!"Multiple metavariables in a pattern are not allowed");
            return false;
        }
        return true;
    }

    if (formula->exprType != pattern->exprType) return false;

    if (pattern->exprType == Expression::Type::BOOLEAN) {
        auto f = std::static_pointer_cast<BooleanFormula>(formula);
        auto p = std::static_pointer_cast<BooleanFormula>(pattern);
        if (f->value != p->value) return false;
    }
    else if (pattern->exprType == Expression::Type::BINARY) {
        auto f = std::static_pointer_cast<BinaryFormula>(formula);
        auto p = std::static_pointer_cast<BinaryFormula>(pattern);
        if (f->op != p->op) return false;
    }
    else if (pattern->exprType == Expression::Type::QUANTIFICATION) {
        auto f = std::static_pointer_cast<QuantificationFormula>(formula);
        auto p = std::static_pointer_cast<QuantificationFormula>(pattern);
        if (f->type != p->type) return false;
        auto it = nameToExpr.find(p->variable->symbol);
        if (it == nameToExpr.end()) {
            nameToExpr[p->variable->symbol] = f->variable;
        }
        else {
            assert(it->second->exprType == Expression::Type::VARIABLE);
            auto s = std::static_pointer_cast<VariableTerm>(it->second)->symbol;
            if (s != f->variable->symbol) return false;
        }
    }
    else if (pattern->exprType == Expression::Type::NEGATION) {
    }
    else {
        assert(!"Unsupported expression type");
        return false;
    }

    size_t count = formula->getChildCount();
    assert(pattern->getChildCount() == count);
    for (size_t i = 0; i < count; ++i) {
        FormulaPtr formulaChild = std::static_pointer_cast<Formula>(formula->getChild(i));
        FormulaPtr patternChild = std::static_pointer_cast<Formula>(pattern->getChild(i));
        if (!matchesRec(formulaChild, patternChild, nameToExpr)) return false;
    }
    return true;
}

FormulaPtr applySubstitutionRec(const FormulaPtr& formula,
    const std::map<std::string, ExpressionPtr>& nameToExpr,
    std::map<std::string, int>& nameUsageCounts) {
    assert(formula && "Formula pointer is null");
    if (!formula) return nullptr;

    if (formula->exprType == Expression::Type::PREDICATE) {
        auto name = std::static_pointer_cast<PredicateFormula>(formula)->symbol;
        auto it = nameToExpr.find(name);
        if (it == nameToExpr.end()) {
            assert(!"Cannot find symbol");
            return nullptr;
        }
        ExpressionPtr result = it->second;
        assert(result->isFormula());
        if (nameUsageCounts[name]++ > 0) {
            result = result->clone();
        }
        return std::static_pointer_cast<Formula>(result);
    }
    else if (formula->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(formula);
        auto it = nameToExpr.find(quant->variable->symbol);
        if (it == nameToExpr.end()) {
            assert(!"Cannot find symbol");
            return nullptr;
        }
        nameUsageCounts[quant->variable->symbol]++;
        quant->variable = std::static_pointer_cast<VariableTerm>(it->second->clone());
    }

    size_t count = formula->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!formula->getChild(i)->isFormula()) {
            assert(!"Replacement formula cannot contain terms");
            return nullptr;
        }
        FormulaPtr child = std::static_pointer_cast<Formula>(formula->getChild(i));
        auto newChild = applySubstitutionRec(child, nameToExpr, nameUsageCounts);
        if (child != newChild) {
            formula->setChild(i, newChild);
        }
    }
    return formula;
}

} // namespace ExpressionRewriter

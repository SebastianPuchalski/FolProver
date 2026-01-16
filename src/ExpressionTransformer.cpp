#include "ExpressionTransformer.hpp"

#include <cassert>

namespace ReplacementRuleDSL {
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
            return ExpressionTransformer::areAlphaEquivalent(it1->second, it2->second);
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
            return !ExpressionTransformer::isVarFreeInExpr(expr, variableSymbol);
            };
        return condition;
    }
} // namespace ReplacementRuleDSL

ExpressionTransformer::ExpressionTransformer() :
    predicateNameCounter(0), functionNameCounter(0), variableNameCounter(0) {
}

bool ExpressionTransformer::isDag(const ExpressionPtr& expr) {
    std::unordered_set<ExpressionPtr> visited;
    std::unordered_set<ExpressionPtr> stack;
    return isDagRec(expr, visited, stack);
}

bool ExpressionTransformer::isTree(const ExpressionPtr& expr) {
    std::unordered_set<ExpressionPtr> visited;
    return isTreeRec(expr, visited);
}

bool ExpressionTransformer::isFullyDefined(const ExpressionPtr& expr) {
    // no empty nodes (nullptr) and empty symbol strings ("")
    std::unordered_set<ExpressionPtr> visited;
    return isFullyDefinedRec(expr, visited);
}

bool ExpressionTransformer::isArityConsistent(const ExpressionPtr& expr) {
    assert(isTree(expr) && isFullyDefined(expr));
    std::unordered_map<std::string, size_t> predArities;
    std::unordered_map<std::string, size_t> funcArities;
    return isArityConsistentRec(expr, predArities, funcArities);
}

bool ExpressionTransformer::isCnf(const FormulaPtr& formula) {
    if (!isTree(formula) || !isFullyDefined(formula)) return false;
    return isCnfRec(formula);
}

bool ExpressionTransformer::isClause(const FormulaPtr& formula) {
    if (!isTree(formula) || !isFullyDefined(formula)) return false;
    return isClauseRec(formula);
}

bool ExpressionTransformer::isJunctionCnf(const FormulaPtr& formula) {
    // Junction AND of junctions OR of literals
    if (!isTree(formula) || !isFullyDefined(formula)) return false;

    if (formula->exprType != Expression::Type::JUNCTION) return false;
    auto junctionAnd = std::static_pointer_cast<JunctionFormula>(formula);
    if (junctionAnd->op != JunctionFormula::Operator::AND) return false;

    for (const auto& clause : junctionAnd->operands) {
        if (clause->exprType != Expression::Type::JUNCTION) return false;
        auto junctionOr = std::static_pointer_cast<JunctionFormula>(clause);
        if (junctionOr->op != JunctionFormula::Operator::OR) return false;
        for (const auto& literal : junctionOr->operands) {
            if (!literal->isLiteral()) return false;
        }
    }
    return true;
}

bool ExpressionTransformer::isNnf(const FormulaPtr& formula) {
    if (!isTree(formula) || !isFullyDefined(formula)) return false;
    return isNnfRec(formula);
}

bool ExpressionTransformer::isStandardized(const FormulaPtr& formula) {
    if (!isTree(formula) || !isFullyDefined(formula)) return false;
    std::vector<std::string> freeVars = getFreeVariables(formula);
    std::unordered_set<std::string> seenNames(freeVars.begin(), freeVars.end());
    return isStandardizedRec(formula, seenNames);
}

bool ExpressionTransformer::isReplacementRuleCorrect(const ReplacementRule& rule) {
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

bool ExpressionTransformer::areReplacementRulesCorrect(
    const std::vector<ReplacementRule>& rules) {
    for (const auto& rule : rules) {
        if (!isReplacementRuleCorrect(rule)) return false;
    }
    return true;
}

bool ExpressionTransformer::areAlphaEquivalent(const ExpressionPtr& expr1, const ExpressionPtr& expr2) {
    assert(isDag(expr1) && isFullyDefined(expr1)); // may cause slowdown in debug mode
    assert(isDag(expr2) && isFullyDefined(expr2)); // may cause slowdown in debug mode
    std::map<std::string, std::string> alphaMap;
    return areAlphaEquivalentRec(expr1, expr2, alphaMap);
}

bool ExpressionTransformer::isVarFreeInExpr(const ExpressionPtr& expr,
    const std::string& varSymbol) {
    assert(isTree(expr) && isFullyDefined(expr)); // may cause slowdown in debug mode
    return isVarFreeInExprRec(expr, varSymbol);
}

std::vector<std::string> ExpressionTransformer::getFreeVariables(const FormulaPtr& formula) {
    assert(isTree(formula) && isFullyDefined(formula));
    std::unordered_map<ExpressionPtr, std::set<std::string>> cache;
    buildFreeVarsCacheRec(formula, cache);
    const auto& varsSet = cache[formula];
    return std::vector<std::string>(varsSet.begin(), varsSet.end());
}

size_t ExpressionTransformer::getExpressionSize(const ExpressionPtr& expr) {
    assert(isDag(expr));
    std::unordered_set<ExpressionPtr> visited;
    return getExpressionSizeRec(expr, visited);
}

void ExpressionTransformer::reserveExpressionSymbols(const ExpressionPtr& expr) {
    assert(isDag(expr));
    std::unordered_set<ExpressionPtr> visited;
    return reserveExpressionSymbolsRec(expr, visited);
}

std::vector<FormulaPtr> ExpressionTransformer::toCnf(const FormulaPtr& formula) {
    using namespace ReplacementRuleDSL;
    assert(isDag(formula));
    assert(isFullyDefined(formula));
    assert(isArityConsistent(formula));

    resetVariableNameGen();
    auto current = std::static_pointer_cast<Formula>(formula->clone());

    auto X = Variable("x");
    auto A = Metavariable("A");
    auto B = Metavariable("B");
    auto C = Metavariable("C");

    // Elimination of junctions
    current = eliminateJunction(current, true);

    // Elimination of implications, equivalences, and exclusive ORs
    std::vector<ReplacementRule> arrowRules = {
        // A => B  -->  ~A | B
        { Imp(A, B), Or(Not(A), B) },
        // A <=> B  -->  (~A | B) & (~B | A)
        { Eqv(A, B), And(Or(Not(A), B), Or(Not(B), A)) },
        // A <~> B  -->  (A | B) & (~A | ~B)
        { Xor(A, B), And(Or(A, B), Or(Not(A), Not(B))) }
    };
    current = rewriteFast(current, arrowRules, true);

    // Boolean simplification
    std::vector<ReplacementRule> boolRules = {
        // ~True --> False               // ~False --> True
        { Not(True()), False() },        { Not(False()), True() },
        // A & True --> A                // True & A --> A
        { And(A, True()), A },           { And(True(), A), A },
        // A & False --> False           // False & A --> False
        { And(A, False()), False() },    { And(False(), A), False() },
        // A | False --> A               // False | A --> A
        { Or(A, False()), A },           { Or(False(), A), A },
        // A | True --> True             // True | A --> True
        { Or(A, True()), True() },       { Or(True(), A), True() },
        // !X.True --> True              // !X.False --> False
        { Forall(X, True()), True() }, { Forall(X, False()), False() },
        // ?X.True --> True              // ?X.False --> False
        { Exists(X, True()), True() }, { Exists(X, False()), False() }
    };
    current = rewriteFast(current, boolRules, true);

    // Conversion to Negation Normal Form (NNF)
    std::vector<ReplacementRule> nnfRules = {
        // ~~A  -->  A
        { Not(Not(A)), A },
        // ~(A & B)  -->  ~A | ~B
        { Not(And(A, B)), Or(Not(A), Not(B)) },
        // ~(A | B)  -->  ~A & ~B
        { Not(Or(A, B)), And(Not(A), Not(B)) },
        // ~!X.A  -->  ?X.~A
        { Not(Forall(X, A)), Exists(X, Not(A)) },
        // ~?X.A  -->  !X.~A
        { Not(Exists(X, A)), Forall(X, Not(A)) }
    };
    current = rewriteFast(current, nnfRules, true);
    assert(isNnf(current));

    // Miniscoping
    std::vector<ReplacementRule> miniScopeRules = {
        // 1. Standard distributivity
        // !X.(A & B) --> (!X.A) & (!X.B)
        { Forall(X, And(A, B)), And(Forall(X, A), Forall(X, B)) },
        // ?X.(A | B) --> (?X.A) | (?X.B)
        { Exists(X, Or(A, B)),  Or(Exists(X, A), Exists(X, B))  },

        // 2. Scope extrusion (pushing quantifiers deeper)
        // !X.(A | B) --> A | (!X.B)      (condition: X is not free in A)
        { Forall(X, Or(A, B)), Or(A, Forall(X, B)), { NotFreeIn(X, A) } },
        // !X.(A | B) --> (!X.A) | B      (condition: X is not free in B)
        { Forall(X, Or(A, B)), Or(Forall(X, A), B), { NotFreeIn(X, B) } },
        // ?X.(A & B) --> A & (?X.B)      (condition: X is not free in A)
        { Exists(X, And(A, B)), And(A, Exists(X, B)), { NotFreeIn(X, A) } },
        // ?X.(A & B) --> (?X.A) & B      (condition: X is not free in B)
        { Exists(X, And(A, B)), And(Exists(X, A), B), { NotFreeIn(X, B) } },

        // 3. Vacuous quantification (removing unused quantifiers)
        // !X.A --> A                     (condition: X is not free in A)
        { Forall(X, A), A, { NotFreeIn(X, A) } },
        // ?X.A --> A                     (condition: X is not free in A)
        { Exists(X, A), A, { NotFreeIn(X, A) } }
    };
    current = rewriteFast(current, miniScopeRules, true);

    // Variable standardization
    if (!isStandardized(current)) {
        current = standardizeVariables(current, true);
    }

    // Skolemization
    current = skolemize(current, true);

    // Elimination of universal quantifiers
    current = eliminateQuantifiers(current, QuantificationFormula::Quantifier::FORALL, true);

    // Distribution of AND over OR (CNF) with Alpha-Equivalence optimizations
    auto A_prime = Metavariable("A_prime");
    std::vector<ReplacementRule> optDistRules = {
        // 1. Boolean simplification (Cleaning up constants)
        // A | True  -->  True
        { Or(A, True()), True() },
        // True | A  -->  True
        { Or(True(), A), True() },
        // A | False  -->  A
        { Or(A, False()), A },
        // False | A  -->  A
        { Or(False(), A), A },

        // 2. Tautology elimination (with Alpha-Equivalence)
        // A | ~A  -->  True
        { Or(A, Not(A_prime)), True(), { AreAlphaEquivalent(A, A_prime) } },
        // ~A | A  -->  True
        { Or(Not(A), A_prime), True(), { AreAlphaEquivalent(A, A_prime) } },

        // 3. Absorption Laws (The "Explosion Killers" with Alpha-Equivalence)
        // A | (A & B)  -->  A
        { Or(A, And(A_prime, B)), A, { AreAlphaEquivalent(A, A_prime) } },
        // (A & B) | A  -->  A
        { Or(And(A_prime, B), A), A, { AreAlphaEquivalent(A, A_prime) } },
        // A | (B & A)  -->  A
        { Or(A, And(B, A_prime)), A, { AreAlphaEquivalent(A, A_prime) } },
        // (B & A) | A  -->  A
        { Or(And(B, A_prime), A), A, { AreAlphaEquivalent(A, A_prime) } },

        // 4. Advanced Absorption (Optional but recommended)
        // A | (~A & B)  -->  A | B
        { Or(A, And(Not(A_prime), B)), Or(A, B), { AreAlphaEquivalent(A, A_prime) } },
        // (~A & B) | A  -->  B | A
        { Or(And(Not(A_prime), B), A), Or(B, A), { AreAlphaEquivalent(A, A_prime) } },

        // Consensus optimization (The "Shortcut")
        // (A & B) | (~A & C) --> (A | C) & (~A | B)
        {
            Or(And(A, B), And(Not(A_prime), C)),
            And(Or(A, C), Or(Not(A_prime), B)),
            { AreAlphaEquivalent(A, A_prime) }
        },
        // (~A & B) | (A & C) --> (~A | C) & (A | B)
        {
            Or(And(Not(A), B), And(A_prime, C)),
            And(Or(Not(A), C), Or(A_prime, B)),
            { AreAlphaEquivalent(A, A_prime) }
        },

        // 6. Proper Distribution (The actual CNF conversion)
        // A | (B & C)  -->  (A | B) & (A | C)
        { Or(A, And(B, C)), And(Or(A, B), Or(A, C)) },
        // (B & C) | A  -->  (B | A) & (C | A)
        { Or(And(B, C), A), And(Or(B, A), Or(C, A)) }
    };
    current = rewriteFast(current, optDistRules, true);

    // Flattening
    auto junction = flattenToJunction(current, JunctionFormula::Operator::AND, true);
    for (auto& clause : junction->operands) {
        clause = flattenToJunction(clause, JunctionFormula::Operator::OR, true);
    }
    assert(isJunctionCnf(junction));
    return junction->operands;
}

FormulaPtr ExpressionTransformer::eliminateJunction(const FormulaPtr& formula, bool inPlace) {
    assert(isDag(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    std::unordered_map<ExpressionPtr, ExpressionPtr> visited;
    return std::static_pointer_cast<Formula>(eliminateJunctionRec(f, visited));
}

FormulaPtr ExpressionTransformer::standardizeVariables(const FormulaPtr& formula, bool inPlace) {
    assert(isTree(formula) && isFullyDefined(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    std::unordered_map<std::string, std::string> nameMap;
    standardizeVariablesRec(f, nameMap);
    assert(isStandardized(f));
    return f;
}

FormulaPtr ExpressionTransformer::skolemize(const FormulaPtr& formula, bool inPlace) {
    assert(isNnf(formula));
    assert(isStandardized(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    std::unordered_map<ExpressionPtr, std::set<std::string>> cache;
    buildFreeVarsCacheRec(f, cache);
    std::unordered_set<std::string> scope;
    std::unordered_map<std::string, TermPtr> replacements;
    const auto& globalFreeVars = cache[f];
    scope.insert(globalFreeVars.begin(), globalFreeVars.end());
    auto result = skolemizeRec(f, scope, replacements, cache);
    return std::static_pointer_cast<Formula>(result);
}

FormulaPtr ExpressionTransformer::eliminateQuantifiers(const FormulaPtr& formula,
    QuantificationFormula::Quantifier typeToRemove, bool inPlace) {
    assert(isStandardized(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    auto processedFormula = eliminateQuantifiersRec(f, typeToRemove);
    return std::static_pointer_cast<Formula>(processedFormula);
}

FormulaPtr ExpressionTransformer::rewrite(const FormulaPtr& formula,
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

FormulaPtr ExpressionTransformer::rewriteFast(const FormulaPtr& formula,
    const std::vector<ReplacementRule>& rules, bool inPlace) {
    assert(isTree(formula) && isFullyDefined(formula));
    assert(isArityConsistent(formula));
    assert(areReplacementRulesCorrect(rules));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    std::unordered_set<ExpressionPtr> checkedNodes;
    return rewriteFastRec(f, rules, checkedNodes);
}

JunctionFormulaPtr ExpressionTransformer::flattenToJunction(const FormulaPtr& formula,
    JunctionFormula::Operator targetOp, bool inPlace) {
    assert(isTree(formula) && isFullyDefined(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    std::vector<FormulaPtr> operands;
    collectOperandsRec(f, targetOp, operands);
    return std::make_shared<JunctionFormula>(targetOp, operands);
}

std::string ExpressionTransformer::getUniquePredicateName() {
    std::string name;
    do {
        name = predicateNamePrefix + std::to_string(++predicateNameCounter);
    } while(reservedPredicateNames.count(name));
    reservedPredicateNames.emplace(name, true);
    return name;
}

std::string ExpressionTransformer::getUniqueFunctionName() {
    std::string name;
    do {
        name = functionNamePrefix + std::to_string(++functionNameCounter);
    } while (reservedFunctionNames.count(name));
    reservedFunctionNames.emplace(name, true);
    return name;
}

std::string ExpressionTransformer::getUniqueVariableName() {
    return variableNamePrefix + std::to_string(++variableNameCounter);
}

void ExpressionTransformer::resetVariableNameGen() {
    variableNameCounter = 0;
}

bool ExpressionTransformer::isDagRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited, std::unordered_set<ExpressionPtr>& stack) {
    if (!expr) return true;

    if (stack.count(expr)) return false;
    if (visited.count(expr)) return true;
    stack.insert(expr);
    visited.insert(expr);

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isDagRec(expr->getChild(i), visited, stack)) return false;
    }

    stack.erase(expr);
    return true;
}

bool ExpressionTransformer::isTreeRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited) {
    if (!expr) return true;

    if (visited.count(expr)) return false;
    visited.insert(expr);

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        if (!isTreeRec(quant->variable, visited)) return false;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isTreeRec(expr->getChild(i), visited)) return false;
    }
    return true;
}

bool ExpressionTransformer::isFullyDefinedRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited) {
    if (!expr) return false;

    if (visited.count(expr)) return true;
    visited.insert(expr);

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        if (!isFullyDefinedRec(quant->variable, visited)) return false;
    }

    if (expr->exprType == Expression::Type::PREDICATE) {
        auto pred = std::static_pointer_cast<PredicateFormula>(expr);
        if (pred->symbol.empty()) return false;
    }
    else if (expr->exprType == Expression::Type::FUNCTION) {
        auto func = std::static_pointer_cast<FunctionTerm>(expr);
        if (func->symbol.empty()) return false;
    }
    else if (expr->exprType == Expression::Type::VARIABLE) {
        auto var = std::static_pointer_cast<VariableTerm>(expr);
        if (var->symbol.empty()) return false;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isFullyDefinedRec(expr->getChild(i), visited)) return false;
    }
    return true;
}

bool ExpressionTransformer::isArityConsistentRec(const ExpressionPtr& expr,
    std::unordered_map<std::string, size_t>& predArities,
    std::unordered_map<std::string, size_t>& funcArities) {
    if (!expr) return true;

    if (expr->exprType == Expression::Type::PREDICATE) {
        auto pred = std::static_pointer_cast<PredicateFormula>(expr);
        size_t currentArity = pred->getChildCount();
        auto it = predArities.find(pred->symbol);
        if (it != predArities.end()) {
            if (it->second != currentArity) return false;
        }
        else {
            predArities[pred->symbol] = currentArity;
        }
    }
    else if (expr->exprType == Expression::Type::FUNCTION) {
        auto func = std::static_pointer_cast<FunctionTerm>(expr);
        if (func->distinct) return true; // always 0
        size_t currentArity = func->getChildCount();
        auto it = funcArities.find(func->symbol);
        if (it != funcArities.end()) {
            if (it->second != currentArity) return false;
        }
        else {
            funcArities[func->symbol] = currentArity;
        }
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isArityConsistentRec(expr->getChild(i), predArities, funcArities)) {
            return false;
        }
    }
    return true;
}

bool ExpressionTransformer::isCnfRec(const FormulaPtr& formula) {
    assert(formula);
    if (!formula) return false;

    if (formula->exprType == Expression::Type::JUNCTION) {
        auto junction = std::static_pointer_cast<JunctionFormula>(formula);
        if (junction->op == JunctionFormula::Operator::AND) {
            for (const auto& child : junction->operands) {
                if (!isCnfRec(child)) return false;
            }
            return true;
        }
    }
    else if (formula->exprType == Expression::Type::BINARY) {
        auto binary = std::static_pointer_cast<BinaryFormula>(formula);
        if (binary->op == BinaryFormula::Operator::AND) {
            return isCnfRec(binary->left) && isCnfRec(binary->right);
        }
    }
    return isClauseRec(formula);
}

bool ExpressionTransformer::isClauseRec(const FormulaPtr& formula) {
    assert(formula);
    if (!formula) return false;

    if (formula->exprType == Expression::Type::JUNCTION) {
        auto junction = std::static_pointer_cast<JunctionFormula>(formula);
        if (junction->op == JunctionFormula::Operator::OR) {
            for (const auto& child : junction->operands) {
                if (!isClauseRec(child)) return false;
            }
            return true;
        }
    }
    else if (formula->exprType == Expression::Type::BINARY) {
        auto binary = std::static_pointer_cast<BinaryFormula>(formula);
        if (binary->op == BinaryFormula::Operator::OR) {
            return isClauseRec(binary->left) && isClauseRec(binary->right);
        }
    }
    return formula->isLiteral();
}

bool ExpressionTransformer::isNnfRec(const FormulaPtr& formula) {
    assert(formula && "Formula pointer is null");
    if (!formula) return false;

    if (formula->isAtom()) return true;
    if (formula->exprType == Expression::Type::NEGATION) {
        auto neg = std::static_pointer_cast<NegationFormula>(formula);
        return neg->child->isAtom();
    }
    if (formula->exprType == Expression::Type::BINARY) {
        auto bin = std::static_pointer_cast<BinaryFormula>(formula);
        if (bin->op != BinaryFormula::Operator::AND &&
            bin->op != BinaryFormula::Operator::OR) {
            return false;
        }
    }

    size_t count = formula->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        auto child = formula->getChild(i);
        if (child->isFormula()) {
            if (!isNnfRec(std::static_pointer_cast<Formula>(child))) return false;
        }
    }
    return true;
}

bool ExpressionTransformer::isStandardizedRec(const ExpressionPtr& expr,
    std::unordered_set<std::string>& seenNames) {
    assert(expr && "Expression pointer is null");
    if (!expr) return false;

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        const std::string& varSymbol = quant->variable->symbol;
        if (seenNames.count(varSymbol)) {
            return false;
        }
        seenNames.insert(varSymbol);
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isStandardizedRec(expr->getChild(i), seenNames)) {
            return false;
        }
    }
    return true;
}

bool ExpressionTransformer::isReplacementRuleCorrectRec(const ReplacementRuleDSL::F& f,
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

bool ExpressionTransformer::areAlphaEquivalentRec(const ExpressionPtr& expr1,
    const ExpressionPtr& expr2, std::map<std::string, std::string>& alphaMap) {
    if (expr1 == expr2) return true;
    if (!expr1 || !expr2) return false;
    if (expr1->exprType != expr2->exprType) return false;

    switch (expr1->exprType) {
    case Expression::Type::BOOLEAN:
        if (std::static_pointer_cast<BooleanFormula>(expr1)->value !=
            std::static_pointer_cast<BooleanFormula>(expr2)->value) return false;
        break;
    case Expression::Type::NEGATION:
        break;
    case Expression::Type::BINARY:
        if (std::static_pointer_cast<BinaryFormula>(expr1)->op !=
            std::static_pointer_cast<BinaryFormula>(expr2)->op) return false;
        break;
    case Expression::Type::JUNCTION:
        if (std::static_pointer_cast<JunctionFormula>(expr1)->op !=
            std::static_pointer_cast<JunctionFormula>(expr2)->op) return false;
        break;
    case Expression::Type::QUANTIFICATION: {
        auto quant1 = std::static_pointer_cast<QuantificationFormula>(expr1);
        auto quant2 = std::static_pointer_cast<QuantificationFormula>(expr2);
        if (quant1->type != quant2->type) return false;
        const std::string& symbol1 = quant1->variable->symbol;
        const std::string& symbol2 = quant2->variable->symbol;
        std::string oldMapping;
        if (auto it = alphaMap.find(symbol1); it != alphaMap.end()) {
            oldMapping = it->second;
        }
        alphaMap[symbol1] = symbol2;
        bool result = areAlphaEquivalentRec(quant1->body, quant2->body, alphaMap);
        if (!oldMapping.empty()) alphaMap[symbol1] = oldMapping;
        else alphaMap.erase(symbol1);
        return result;
    }
    case Expression::Type::PREDICATE:
        if (std::static_pointer_cast<PredicateFormula>(expr1)->symbol !=
            std::static_pointer_cast<PredicateFormula>(expr2)->symbol) return false;
        break;
    case Expression::Type::FUNCTION: {
        auto func1 = std::static_pointer_cast<FunctionTerm>(expr1);
        auto func2 = std::static_pointer_cast<FunctionTerm>(expr2);
        if (func1->symbol != func2->symbol) return false;
        if (func1->distinct != func2->distinct) return false;
        break;
    }
    case Expression::Type::VARIABLE: {
        const auto& var1Symbol = std::static_pointer_cast<VariableTerm>(expr1)->symbol;
        const auto& var2Symbol = std::static_pointer_cast<VariableTerm>(expr2)->symbol;
        if (auto it = alphaMap.find(var1Symbol); it != alphaMap.end()) {
            return it->second == var2Symbol;
        }
        return var1Symbol == var2Symbol;
    }
    default:
        assert(!"Unsupported expression type");
        break;
    }

    size_t count = expr1->getChildCount();
    if (count != expr2->getChildCount()) return false;
    for (size_t i = 0; i < count; ++i) {
        if (!areAlphaEquivalentRec(expr1->getChild(i), expr2->getChild(i), alphaMap)) return false;
    }
    return true;
}

bool ExpressionTransformer::isVarFreeInExprRec(const ExpressionPtr& expr,
    const std::string& varSymbol) {
    if (!expr) return false;

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        if (quant->variable->symbol == varSymbol) {
            return false;
        }
    }
    else if (expr->exprType == Expression::Type::VARIABLE) {
        auto var = std::static_pointer_cast<VariableTerm>(expr);
        return var->symbol == varSymbol;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (isVarFreeInExprRec(expr->getChild(i), varSymbol)) return true;
    }
    return false;
}

void ExpressionTransformer::buildFreeVarsCacheRec(const ExpressionPtr& expr,
    std::unordered_map<ExpressionPtr, std::set<std::string>>& cache) {
    assert(expr && "Expression pointer is null");
    if (!expr || cache.count(expr)) return;

    std::set<std::string> vars;

    if (expr->exprType == Expression::Type::VARIABLE) {
        auto var = std::static_pointer_cast<VariableTerm>(expr);
        vars.insert(var->symbol);
    }
    else if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        buildFreeVarsCacheRec(quant->body, cache);
        const auto& bodyVars = cache[quant->body];
        vars.insert(bodyVars.begin(), bodyVars.end());
        vars.erase(quant->variable->symbol);
    }
    else {
        size_t count = expr->getChildCount();
        for (size_t i = 0; i < count; ++i) {
            auto child = expr->getChild(i);
            buildFreeVarsCacheRec(child, cache);
            const auto& childVars = cache[child];
            vars.insert(childVars.begin(), childVars.end());
        }
    }
    cache[expr] = std::move(vars);
}

size_t ExpressionTransformer::getExpressionSizeRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited) {
    if (!expr || visited.count(expr)) return 0;
    visited.insert(expr);
    size_t size = 1;
    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        size += getExpressionSizeRec(quant->variable, visited);
    }
    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        size += getExpressionSizeRec(expr->getChild(i), visited);
    }
    return size;
}

void ExpressionTransformer::reserveExpressionSymbolsRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited) {
    if (!expr || visited.count(expr)) return;
    visited.insert(expr);
    if (expr->exprType == Expression::Type::PREDICATE) {
        auto pred = std::static_pointer_cast<PredicateFormula>(expr);
        auto it = reservedPredicateNames.find(pred->symbol);
        if (it != reservedPredicateNames.end() && it->second) {
            assert(!"Predicate symbol name already reserved");
            // reserve all symbols before getUniquePredicateName()
        }
        reservedPredicateNames.emplace(pred->symbol, false);
    }
    else if (expr->exprType == Expression::Type::FUNCTION) {
        auto func = std::static_pointer_cast<FunctionTerm>(expr);
        if (!func->distinct) {
            auto it = reservedFunctionNames.find(func->symbol);
            if (it != reservedFunctionNames.end() && it->second) {
                assert(!"Function symbol name already reserved");
                // reserve all symbols before getUniqueFunctionName()
            }
            reservedFunctionNames.emplace(func->symbol, false);
        }
    }
    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        reserveExpressionSymbolsRec(expr->getChild(i), visited);
    }
}

ExpressionPtr ExpressionTransformer::eliminateJunctionRec(const ExpressionPtr& expr,
    std::unordered_map<ExpressionPtr, ExpressionPtr>& visited) {
    if (!expr) return nullptr;
    if (auto it = visited.find(expr); it != visited.end()) return it->second;
    if (!expr->isFormula()) return expr;

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        auto child = expr->getChild(i);
        auto newChild = eliminateJunctionRec(child, visited);
        if (child != newChild) {
            expr->setChild(i, newChild);
        }
    }

    ExpressionPtr result = expr;
    if (expr->exprType == Expression::Type::JUNCTION) {
        auto junction = std::static_pointer_cast<JunctionFormula>(expr);
        const auto& operands = junction->operands;
        if (operands.empty()) {
            result = std::make_shared<BooleanFormula>(
                junction->op == JunctionFormula::Operator::AND);
        }
        else {
            auto binaryOp = (junction->op == JunctionFormula::Operator::AND)
                ? BinaryFormula::Operator::AND : BinaryFormula::Operator::OR;
            result = operands.front();
            for (size_t i = 1; i < operands.size(); ++i) {
                result = std::make_shared<BinaryFormula>(
                    binaryOp,
                    std::static_pointer_cast<Formula>(result),
                    std::static_pointer_cast<Formula>(operands[i])
                );
            }
        }
    }
    visited[expr] = result;
    return result;
}

void ExpressionTransformer::standardizeVariablesRec(const ExpressionPtr& expr,
    std::unordered_map<std::string, std::string>& nameMap) {
    assert(expr && "Expression pointer is null");
    if (!expr) return;

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        std::string oldName = quant->variable->symbol;
        assert(!oldName.empty());
        std::string newName = getUniqueVariableName();
        quant->variable->symbol = newName;
        std::string backupName;
        if (auto it = nameMap.find(oldName); it != nameMap.end()) {
            backupName = it->second;
        }
        nameMap[oldName] = newName;
        standardizeVariablesRec(quant->body, nameMap);
        if (!backupName.empty()) nameMap[oldName] = backupName;
        else nameMap.erase(oldName);
        return;
    }

    if (expr->exprType == Expression::Type::VARIABLE) {
        auto var = std::static_pointer_cast<VariableTerm>(expr);
        assert(!var->symbol.empty());
        auto it = nameMap.find(var->symbol);
        if (it != nameMap.end()) {
            var->symbol = it->second;
        }
        else {
            std::string newName = getUniqueVariableName();
            nameMap[var->symbol] = newName;
            var->symbol = newName;
        }
        return;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child = expr->getChild(i);
        standardizeVariablesRec(child, nameMap);
    }
}

ExpressionPtr ExpressionTransformer::skolemizeRec(const ExpressionPtr& expr,
    std::unordered_set<std::string>& universalVars,
    std::unordered_map<std::string, TermPtr>& substitutions,
    std::unordered_map<ExpressionPtr, std::set<std::string>>& cache) {
    assert(expr && "Expression pointer is null");
    if (!expr) return nullptr;

    if (expr->exprType == Expression::Type::FUNCTION) {
        auto func = std::static_pointer_cast<FunctionTerm>(expr);
        if (!func->distinct) {
            // sanity check: guard against missing symbol reservation
            // make sure all symbols are reserved before skolemization
            assert(reservedFunctionNames.count(func->symbol));
        }
    }

    if (expr->exprType == Expression::Type::VARIABLE) {
        auto var = std::static_pointer_cast<VariableTerm>(expr);
        auto it = substitutions.find(var->symbol);
        if (it != substitutions.end()) {
            return it->second->clone();
        }
        return expr;
    }

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);

        if (quant->type == QuantificationFormula::Quantifier::FORALL) {
            std::string varName = quant->variable->symbol;
            universalVars.insert(varName);
            ExpressionPtr newBody = skolemizeRec(quant->body, universalVars, substitutions, cache);
            quant->body = std::static_pointer_cast<Formula>(newBody);
            universalVars.erase(varName);
            return quant;
        }
        else {
            std::vector<TermPtr> skolemArgs;
            const auto& freeVarNames = cache.at(quant->body);
            for (const auto& freeVarName : freeVarNames) {
                if (universalVars.find(freeVarName) != universalVars.end()) {
                    skolemArgs.push_back(std::make_shared<VariableTerm>(freeVarName));
                }
            }
            auto skolemTerm = std::make_shared<FunctionTerm>(getUniqueFunctionName(), skolemArgs);
            substitutions[quant->variable->symbol] = skolemTerm;
            ExpressionPtr result = skolemizeRec(quant->body, universalVars, substitutions, cache);
            substitutions.erase(quant->variable->symbol);
            return result;
        }
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child = expr->getChild(i);
        ExpressionPtr newChild = skolemizeRec(child, universalVars, substitutions, cache);
        if (child != newChild) {
            expr->setChild(i, newChild);
        }
    }
    return expr;
}

ExpressionPtr ExpressionTransformer::eliminateQuantifiersRec(const ExpressionPtr& expr,
    QuantificationFormula::Quantifier typeToRemove) {
    assert(expr && "Expression pointer is null");
    if (!expr) return nullptr;

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        ExpressionPtr processedBody = eliminateQuantifiersRec(quant->body, typeToRemove);
        if (quant->type == typeToRemove) {
            return processedBody;
        }
        quant->body = std::static_pointer_cast<Formula>(processedBody);
        return quant;
    }

    if (expr->isFormula()) {
        if (std::static_pointer_cast<Formula>(expr)->isAtom()) return expr;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child = expr->getChild(i);
        ExpressionPtr newChild = eliminateQuantifiersRec(child, typeToRemove);
        if (child != newChild) {
            expr->setChild(i, newChild);
        }
    }
    return expr;
}

FormulaPtr ExpressionTransformer::rewriteRec(const FormulaPtr& formula,
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

FormulaPtr ExpressionTransformer::rewriteFastRec(const FormulaPtr& formula,
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

bool ExpressionTransformer::matchesRec(const FormulaPtr& formula, const FormulaPtr& pattern,
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

    if(pattern->exprType == Expression::Type::BOOLEAN) {
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

FormulaPtr ExpressionTransformer::applySubstitutionRec(const FormulaPtr& formula,
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

void ExpressionTransformer::collectOperandsRec(const FormulaPtr& formula,
    JunctionFormula::Operator targetOp,
    std::vector<FormulaPtr>& accumulator) {
    assert(formula && "Formula pointer is null");
    if (!formula) return;

    if (formula->exprType == Expression::Type::BINARY) {
        auto binary = std::static_pointer_cast<BinaryFormula>(formula);
        if ((targetOp == JunctionFormula::Operator::AND && binary->op == BinaryFormula::Operator::AND) ||
            (targetOp == JunctionFormula::Operator::OR && binary->op == BinaryFormula::Operator::OR)) {
            collectOperandsRec(binary->left, targetOp, accumulator);
            collectOperandsRec(binary->right, targetOp, accumulator);
            return;
        }
    }

    if (formula->exprType == Expression::Type::JUNCTION) {
        auto junction = std::static_pointer_cast<JunctionFormula>(formula);
        if (junction->op == targetOp) {
            for (const auto& op : junction->operands) {
                collectOperandsRec(op, targetOp, accumulator);
            }
            return;
        }
    }

    accumulator.push_back(formula);
}

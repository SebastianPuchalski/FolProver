#include "EquationalLogicTranslator.hpp"

#include "ExpressionBuilder.hpp"

#include <cassert>

std::vector<ProofNodePtr> EquationalLogicTranslator::translateToEquationalLogic(
    const std::vector<ProofNodePtr>& clauseNodes) const {

    std::map<std::string, size_t> functionArities;
    std::map<std::string, size_t> predicateArities;
    std::vector<std::string> distinctObjects;
    bool hasNot, hasEquality;
    extractSymbols(clauseNodes, functionArities, predicateArities, distinctObjects,
        hasNot, hasEquality);

    std::vector<ProofNodePtr> result;

    for (const auto& node : clauseNodes) {
        if (auto translated = translateClause(node)) {
            result.push_back(translated);
        }
    }

    auto booleanAxioms = generateBooleanAxioms();
    result.insert(result.end(), booleanAxioms.begin(), booleanAxioms.end());

    if (distinctObjects.size() > 1) {
        hasEquality = true; // distinct object axioms use eq
        auto distinctAxioms = generateDistinctObjectAxioms(distinctObjects);
        result.insert(result.end(), distinctAxioms.begin(), distinctAxioms.end());
    }

    if (hasEquality) {
        auto equalityAxioms = generateEqualityAxioms(functionArities, predicateArities);
        result.insert(result.end(), equalityAxioms.begin(), equalityAxioms.end());
    }

    auto metaRules = generateMetaRules();
    result.insert(result.end(), metaRules.begin(), metaRules.end());

    return translateToDisjunction(result);
}

void EquationalLogicTranslator::extractSymbols(
    const std::vector<ProofNodePtr>& clauseNodes,
    std::map<std::string, size_t>& functionArities,
    std::map<std::string, size_t>& predicateArities,
    std::vector<std::string>& distinctObjects,
    bool& hasNot, bool& hasEquality) const {
    hasNot = hasEquality = false;

    std::vector<ExpressionPtr> stack;
    for (const auto& node : clauseNodes) {
        if (node && node->getFormula()) {
            stack.push_back(node->getFormula());
        }
    }

    while (!stack.empty()) {
        auto expr = stack.back();
        stack.pop_back();
        if (!expr) continue;

        if (expr->exprType == Expression::Type::NEGATION) {
            hasNot = true;
        }
        else if (expr->exprType == Expression::Type::PREDICATE) {
            auto predicate = std::static_pointer_cast<PredicateFormula>(expr);
            predicateArities[predicate->symbol] = predicate->arguments.size();
        }
        else if (expr->exprType == Expression::Type::EQUALITY) {
            hasEquality = true;
        }
        else if (expr->exprType == Expression::Type::FUNCTION) {
            auto function = std::static_pointer_cast<FunctionTerm>(expr);
            if (function->distinct) {
                if (std::find(distinctObjects.begin(), distinctObjects.end(),
                    function->symbol) == distinctObjects.end()) {
                    distinctObjects.push_back(function->symbol);
                }
            }
            else {
                functionArities[function->symbol] = function->arguments.size();
            }
        }

        size_t childCount = expr->getChildCount();
        for (size_t i = 0; i < childCount; ++i) {
            stack.push_back(expr->getChild(i));
        }
    }
}

ProofNodePtr EquationalLogicTranslator::translateClause(
    const ProofNodePtr& clauseNode) const {
    namespace EB = ExpressionBuilder;
    if (!clauseNode || !clauseNode->getFormula()) return nullptr;
    TermPtr translatedTerm = translateFormulaToTerm(clauseNode->getFormula());
    FormulaPtr equationalFormula = EB::Equal(translatedTerm, EB::Func(SYMBOL_TRUE));
    return ProofStep::create(equationalFormula,
        ProofNode::Type::INFERENCE, "translate_to_equation", { clauseNode });
}

TermPtr EquationalLogicTranslator::translateFormulaToTerm(
    const FormulaPtr& formula) const {
    namespace EB = ExpressionBuilder;
    if (!formula) return nullptr;

    switch (formula->exprType) {
    case Expression::Type::BOOLEAN: {
        auto boolean = std::static_pointer_cast<BooleanFormula>(formula);
        return EB::Func(boolean->value ? SYMBOL_TRUE : SYMBOL_FALSE);
    }
    case Expression::Type::NEGATION: {
        auto negation = std::static_pointer_cast<NegationFormula>(formula);
        auto child = std::static_pointer_cast<Formula>(negation->child);
        auto childTerm = translateFormulaToTerm(child);
        return EB::Func(SYMBOL_NOT, { childTerm });
    }
    case Expression::Type::JUNCTION: {
        auto junction = std::static_pointer_cast<JunctionFormula>(formula);
        assert(junction->op == JunctionFormula::Operator::OR);
        if (junction->operands.empty()) return EB::Func(SYMBOL_FALSE);
        TermPtr result = translateFormulaToTerm(junction->operands.back());
        for (int i = static_cast<int>(junction->operands.size()) - 2; i >= 0; --i) {
            result = EB::Func(SYMBOL_OR, { translateFormulaToTerm(junction->operands[i]), result });
        }
        return result;
    }
    case Expression::Type::PREDICATE: {
        auto predicate = std::static_pointer_cast<PredicateFormula>(formula);
        std::vector<TermPtr> args;
        args.reserve(predicate->arguments.size());
        for (const auto& arg : predicate->arguments) {
            args.push_back(std::static_pointer_cast<Term>(arg->clone()));
        }
        return EB::Func(predicate->symbol, args);
    }
    case Expression::Type::EQUALITY: {
        auto eq = std::static_pointer_cast<EqualityFormula>(formula);
        return EB::Func(SYMBOL_EQ, {
            std::static_pointer_cast<Term>(eq->left->clone()),
            std::static_pointer_cast<Term>(eq->right->clone())});
    }
    default:
        assert(false);
        return nullptr;
    }
}

std::vector<ProofNodePtr> EquationalLogicTranslator::generateBooleanAxioms() const {
    namespace EB = ExpressionBuilder;
    std::vector<ProofNodePtr> axioms;

    auto addAxiom = [&](const TermPtr& left, const TermPtr& right, const std::string& ruleName) {
        axioms.push_back(ProofStep::create(
            EB::Equal(left, right),
            ProofNode::Type::PREMISE,
            ruleName, {}));
    };

    auto x = EB::Var("X");
    auto y = EB::Var("Y");
    auto z = EB::Var("Z");
    auto trueTerm = EB::Func(SYMBOL_TRUE);
    auto falseTerm = EB::Func(SYMBOL_FALSE);

    // OR commutativity: x | y = y | x
    addAxiom(EB::Func(SYMBOL_OR, { x, y }),
        EB::Func(SYMBOL_OR, { y, x }),
        "boolean_or_commutativity");

    // This axiom slows down the system a lot!!!
    // OR associativity: x | (y | z) = (x | y) | z
    addAxiom(EB::Func(SYMBOL_OR, { x, EB::Func(SYMBOL_OR, {y, z}) }),
        EB::Func(SYMBOL_OR, { EB::Func(SYMBOL_OR, {x, y}), z }),
        "boolean_or_associativity");

    // OR idempotency: x | x = x
    addAxiom(EB::Func(SYMBOL_OR, { x, x }),
        x, "boolean_or_idempotency");

    // OR identity: x | F = x
    addAxiom(EB::Func(SYMBOL_OR, { x, falseTerm }),
        x, "boolean_or_identity");

    // OR annihilator: x | T = T
    addAxiom(EB::Func(SYMBOL_OR, { x, trueTerm }),
        trueTerm, "boolean_or_annihilator");

    // Excluded middle: x | ~x = T
    addAxiom(EB::Func(SYMBOL_OR, { x, EB::Func(SYMBOL_NOT, {x}) }),
        trueTerm, "boolean_excluded_middle");

    // Double negation: ~~x = x
    addAxiom(EB::Func(SYMBOL_NOT, { EB::Func(SYMBOL_NOT, {x}) }),
        x, "boolean_double_negation");

    // Truth negation: ~T = F
    addAxiom(EB::Func(SYMBOL_NOT, { trueTerm }),
        falseTerm, "boolean_truth_negation");

    // Falsity negation: ~F = T
    addAxiom(EB::Func(SYMBOL_NOT, { falseTerm }),
        trueTerm, "boolean_falsity_negation");

    return axioms;
}

std::vector<ProofNodePtr> EquationalLogicTranslator::generateEqualityAxioms(
    const std::map<std::string, size_t>& functionArities,
    const std::map<std::string, size_t>& predicateArities) const {
    namespace EB = ExpressionBuilder;
    std::vector<ProofNodePtr> axioms;

    auto addAxiom = [&](const TermPtr& left, const TermPtr& right, const std::string& ruleName) {
        axioms.push_back(ProofStep::create(
            EB::Equal(left, right),
            ProofNode::Type::PREMISE,
            ruleName, {}));
    };

    auto x = EB::Var("X");
    auto y = EB::Var("Y");
    auto trueTerm = EB::Func(SYMBOL_TRUE);

    std::map<std::string, size_t> allArities = functionArities;
    for (const auto& [name, arity] : predicateArities) {
        allArities[name] = arity;
    }
    allArities[SYMBOL_NOT] = 1;
    allArities[SYMBOL_OR] = 2;
    allArities[SYMBOL_EQ] = 2;

    // Reflexivity: eq(x, x) = T
    addAxiom(EB::Func(SYMBOL_EQ, { x, x }),
        EB::Func(SYMBOL_TRUE), "equality_reflexivity");

    if (!useExtractionRule) {
        const std::string SYMBOL_GUARD = "$guard";

        // Guard reduction axiom: g(T, x) = x
        addAxiom(EB::Func(SYMBOL_GUARD, { trueTerm, x }),
            x, "guard_reduction");

        // Congruence: g(eq(x, y), f(..x..)) = g(eq(x, y), f(..y..))
        for (const auto& [symbol, arity] : allArities) {
            if (arity == 0) continue;
            for (size_t i = 0; i < arity; ++i) {
                std::vector<TermPtr> argsLeft;
                std::vector<TermPtr> argsRight;
                for (size_t j = 0; j < arity; ++j) {
                    if (j == i) {
                        argsLeft.push_back(x);
                        argsRight.push_back(y);
                    }
                    else {
                        auto z = EB::Var("Z" + std::to_string(j));
                        argsLeft.push_back(z);
                        argsRight.push_back(z);
                    }
                }

                auto eqTerm = EB::Func(SYMBOL_EQ, { x, y });
                auto funcLeft = EB::Func(symbol, argsLeft);
                auto funcRight = EB::Func(symbol, argsRight);

                auto guardLeft = EB::Func(SYMBOL_GUARD, { eqTerm, funcLeft });
                auto guardRight = EB::Func(SYMBOL_GUARD, { eqTerm, funcRight });

                std::string ruleName = "equality_congruence_" + symbol + "_" + std::to_string(i);
                addAxiom(guardLeft, guardRight, ruleName);
            }
        }
    }

    return axioms;
}

std::vector<ProofNodePtr> EquationalLogicTranslator::generateDistinctObjectAxioms(
    const std::vector<std::string>& distinctObjects) const {
    namespace EB = ExpressionBuilder;
    std::vector<ProofNodePtr> axioms;

    for (size_t i = 0; i < distinctObjects.size(); ++i) {
        for (size_t j = i + 1; j < distinctObjects.size(); ++j) {
            auto d1 = EB::Distinct(distinctObjects[i]);
            auto d2 = EB::Distinct(distinctObjects[j]);
            // ~eq(d1, d2) = T
            auto notEqArgs = EB::Func(SYMBOL_NOT, { EB::Func(SYMBOL_EQ, {d1, d2}) });
            axioms.push_back(ProofStep::create(
                EB::Equal(notEqArgs, EB::Func(SYMBOL_TRUE)),
                ProofNode::Type::PREMISE,
                "distinct_objects_axiom", {}));
        }
    }
    return axioms;
}

std::vector<ProofNodePtr> EquationalLogicTranslator::generateMetaRules() const {
    namespace EB = ExpressionBuilder;
    std::vector<ProofNodePtr> rules;

    auto x = EB::Var("X");
    auto y = EB::Var("Y");
    auto trueTerm = EB::Func(SYMBOL_TRUE);
    auto falseTerm = EB::Func(SYMBOL_FALSE);

    // Meta extraction: ~(eq(x, y) = T) | (x = y)
    if (useExtractionRule) {
        auto eqFuncIsTrue = EB::Equal(EB::Func(SYMBOL_EQ, { x, y }), trueTerm);
        auto nativeEq = EB::Equal(x, y);
        rules.push_back(ProofStep::create(
            EB::Disjunction({ EB::Not(eqFuncIsTrue), nativeEq }),
            ProofNode::Type::PREMISE, "meta_extraction", {}));
    }

    // Meta contradiction: ~(T = F)
    rules.push_back(ProofStep::create(
        EB::Not(EB::Equal(trueTerm, falseTerm)),
        ProofNode::Type::PREMISE, "meta_contradiction", {}));

    return rules;
}

std::vector<ProofNodePtr> EquationalLogicTranslator::translateToDisjunction(
    const std::vector<ProofNodePtr>& nodes) const {
    namespace EB = ExpressionBuilder;
    std::vector<ProofNodePtr> result;
    result.reserve(nodes.size());

    for (const auto& node : nodes) {
        if (!node || !node->getFormula()) continue;
        auto formula = node->getFormula();

        if (formula->exprType == Expression::Type::JUNCTION) {
            auto junction = std::static_pointer_cast<JunctionFormula>(formula);
            assert(junction->op == JunctionFormula::Operator::OR);
            result.push_back(node);
        }
        else {
            auto disjunctionFormula = EB::Disjunction({ formula });
            if (auto step = std::dynamic_pointer_cast<ProofStep>(node)) {
                result.push_back(ProofStep::create(disjunctionFormula,
                    step->getType(), step->getRule(), step->getParents()));
            }
            else {
                result.push_back(ProofStep::create(
                    disjunctionFormula, ProofNode::Type::INFERENCE,
                    "wrap_in_disjunction", { node }));
            }
        }
    }
    return result;
}

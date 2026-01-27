#pragma once

#include "Expression.hpp"

#include <functional>
#include <map>

namespace ExpressionRewriter {

namespace DSL {
    /*
     * Allowed elements:
     * - Boolean constants:  True, False
     * - Logical operators:  Not, And, Or, Imp, Eqv, Xor
     * - Quantifiers:        Forall, Exists
     * - Variables:          variables used in quantifiers
     * - Metavariables:      placeholders for sub-formulas
     *
     * Pattern constraints (left-hand side):
     * - Metavariable names must be unique and appear only once
     * - Quantifier variables can appear multiple times
     * - If a variable name is repeated, the matched formula must also use the
     * same variable symbol in corresponding places
     *
     * Replacement constraints (right-hand side):
     * - Only metavariables and quantifier variables defined in the pattern may be used
     * - Metavariables and variables may be used multiple times
     *
     * Side condition (optional):
     * - Only metavariables and quantifier variables defined in the pattern may be used
     * - AreAlphaEquivalent: checks if two metavariables match alpha-equivalent formulas
     * (structurally equal up to renaming of bound variables)
     * - NotFreeIn: checks if variable is not free in the formula matched by metavariable
     */

    using F = FormulaPtr;
    using V = VariableTermPtr;

    struct Condition {
        std::vector<ExpressionPtr> usedExprs;
        std::function<bool(const std::map<std::string, ExpressionPtr>&)> check;
    };

    F True();
    F False();

    F Not(F f);

    F And(F l, F r);
    F Or(F l, F r);
    F Imp(F l, F r);
    F Eqv(F l, F r);
    F Xor(F l, F r);

    F Forall(V variable, F body);
    F Exists(V variable, F body);

    V Variable(const std::string& name);
    F Metavariable(const std::string& name);

    Condition AreAlphaEquivalent(F metavar1, F metavar2);
    Condition NotFreeIn(V variable, F metavariable);
} // namespace DSL

struct ReplacementRule {
    ReplacementRule(DSL::F p, DSL::F r, std::vector<DSL::Condition> c = {})
        : pattern(p), replacement(r), conditions(std::move(c)) {
    }

    DSL::F pattern;
    DSL::F replacement;
    std::vector<DSL::Condition> conditions;
};

bool isReplacementRuleCorrect(const ReplacementRule& rule);
bool areReplacementRulesCorrect(const std::vector<ReplacementRule>& rules);

FormulaPtr rewrite(const FormulaPtr& formula,
    const std::vector<ReplacementRule>& rules, bool inPlace = false);
FormulaPtr rewriteFast(const FormulaPtr& formula,
    const std::vector<ReplacementRule>& rules, bool inPlace = false);

} // namespace ExpressionRewriter

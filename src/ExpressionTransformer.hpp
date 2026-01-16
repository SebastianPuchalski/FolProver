#pragma once

#include "Expression.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ReplacementRuleDSL {
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
} // namespace ReplacementRuleDSL

class ExpressionTransformer {
public:
    ExpressionTransformer();

    struct ReplacementRule {
        ReplacementRule(ReplacementRuleDSL::F p, ReplacementRuleDSL::F r,
            std::vector<ReplacementRuleDSL::Condition> c = {})
            : pattern(p), replacement(r), conditions(std::move(c)) {
        }

        ReplacementRuleDSL::F pattern;
        ReplacementRuleDSL::F replacement;
        std::vector<ReplacementRuleDSL::Condition> conditions;
    };

    static bool isDag(const ExpressionPtr& expr);
    static bool isTree(const ExpressionPtr& expr);
    static bool isFullyDefined(const ExpressionPtr& expr);
    static bool isArityConsistent(const ExpressionPtr& expr);
    static bool isCnf(const FormulaPtr& formula);
    static bool isClause(const FormulaPtr& formula);
    static bool isJunctionCnf(const FormulaPtr& formula);
    static bool isNnf(const FormulaPtr& formula);
    static bool isStandardized(const FormulaPtr& formula);
    static bool isReplacementRuleCorrect(const ReplacementRule& rule);
    static bool areReplacementRulesCorrect(const std::vector<ReplacementRule>& rules);

    static bool areAlphaEquivalent(const ExpressionPtr& expr1, const ExpressionPtr& expr2);
    static bool isVarFreeInExpr(const ExpressionPtr& expr, const std::string& varSymbol);
    static std::vector<std::string> getFreeVariables(const FormulaPtr& formula);
    static size_t getExpressionSize(const ExpressionPtr& expr);

    void reserveExpressionSymbols(const ExpressionPtr& expr);
    std::vector<FormulaPtr> toCnf(const FormulaPtr& formula);
    static FormulaPtr eliminateJunction(const FormulaPtr& formula, bool inPlace = false);
    FormulaPtr standardizeVariables(const FormulaPtr& formula, bool inPlace = false);
    FormulaPtr skolemize(const FormulaPtr& formula, bool inPlace = false);
    static FormulaPtr eliminateQuantifiers(const FormulaPtr& formula,
        QuantificationFormula::Quantifier typeToRemove, bool inPlace = false);
    static FormulaPtr rewrite(const FormulaPtr& formula,
        const std::vector<ReplacementRule>& rules, bool inPlace = false);
    static FormulaPtr rewriteFast(const FormulaPtr& formula,
        const std::vector<ReplacementRule>& rules, bool inPlace = false);
    static JunctionFormulaPtr flattenToJunction(const FormulaPtr& formula,
        JunctionFormula::Operator targetOp, bool inPlace = false);

private:
    std::string predicateNamePrefix = "p";
    std::string functionNamePrefix = "sk";
    std::string variableNamePrefix = "V";
    uint64_t predicateNameCounter;
    uint64_t functionNameCounter;
    uint64_t variableNameCounter;
    std::unordered_map<std::string, bool> reservedPredicateNames;
    std::unordered_map<std::string, bool> reservedFunctionNames;

    std::string getUniquePredicateName();
    std::string getUniqueFunctionName();
    std::string getUniqueVariableName();
    void resetVariableNameGen();

    static bool isDagRec(const ExpressionPtr& expr,
        std::unordered_set<ExpressionPtr>& visited,
        std::unordered_set<ExpressionPtr>& stack);
    static bool isTreeRec(const ExpressionPtr& expr,
        std::unordered_set<ExpressionPtr>& visited);
    static bool isFullyDefinedRec(const ExpressionPtr& expr,
        std::unordered_set<ExpressionPtr>& visited);
    static bool isArityConsistentRec(const ExpressionPtr& expr,
        std::unordered_map<std::string, size_t>& predArities,
        std::unordered_map<std::string, size_t>& funcArities);
    static bool isCnfRec(const FormulaPtr& formula);
    static bool isClauseRec(const FormulaPtr& formula);
    static bool isNnfRec(const FormulaPtr& formula);
    static bool isStandardizedRec(const ExpressionPtr& expr,
        std::unordered_set<std::string>& seenNames);
    static bool isReplacementRuleCorrectRec(const ReplacementRuleDSL::F& f,
        std::unordered_set<std::string>& usedMetavarSymbols,
        std::unordered_set<std::string>& usedVarSymbols, bool pattern);

    static bool areAlphaEquivalentRec(const ExpressionPtr& expr1,
        const ExpressionPtr& expr2, std::map<std::string, std::string>& alphaMap);
    static bool isVarFreeInExprRec(const ExpressionPtr& expr,
        const std::string& varSymbol);
    static void buildFreeVarsCacheRec(const ExpressionPtr& expr,
        std::unordered_map<ExpressionPtr, std::set<std::string>>& cache);
    static size_t getExpressionSizeRec(const ExpressionPtr& expr,
        std::unordered_set<ExpressionPtr>& visited);

    void reserveExpressionSymbolsRec(const ExpressionPtr& expr,
        std::unordered_set<ExpressionPtr>& visited);
    static ExpressionPtr eliminateJunctionRec(const ExpressionPtr& expr,
        std::unordered_map<ExpressionPtr, ExpressionPtr>& visited);
    void standardizeVariablesRec(const ExpressionPtr& expr,
        std::unordered_map<std::string, std::string>& nameMap);
    ExpressionPtr skolemizeRec(const ExpressionPtr& expr,
        std::unordered_set<std::string>& universalVars,
        std::unordered_map<std::string, TermPtr>& substitutions,
        std::unordered_map<ExpressionPtr, std::set<std::string>>& cache);
    static ExpressionPtr eliminateQuantifiersRec(const ExpressionPtr& expr,
        QuantificationFormula::Quantifier typeToRemove);

    static FormulaPtr rewriteRec(const FormulaPtr& formula,
        const std::vector<ReplacementRule>& rules, bool& anyChange);
    static FormulaPtr rewriteFastRec(const FormulaPtr& formula,
        const std::vector<ReplacementRule>& rules,
        std::unordered_set<ExpressionPtr>& checkedNodes);
    static bool matchesRec(const FormulaPtr& formula, const FormulaPtr& pattern,
        std::map<std::string, ExpressionPtr>& nameToExpr);
    static FormulaPtr applySubstitutionRec(const FormulaPtr& formula,
        const std::map<std::string, ExpressionPtr>& nameToExpr,
        std::map<std::string, int>& nameUsageCounts);

    static void collectOperandsRec(const FormulaPtr& formula,
        JunctionFormula::Operator targetOp,
        std::vector<FormulaPtr>& accumulator);
};

#pragma once

#include "Expression.hpp"
#include "NameRegistry.hpp"

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ExpressionTransformer {
public:
    ExpressionTransformer(std::shared_ptr<NameRegistry> nameRegistry = nullptr);

    std::shared_ptr<NameRegistry> getNameRegistry() const { return nameRegistry; }

    std::vector<FormulaPtr> toCnf(const FormulaPtr& formula);
    static FormulaPtr eliminateJunction(const FormulaPtr& formula, bool inPlace = false);
    FormulaPtr standardizeVariables(const FormulaPtr& formula, bool inPlace = false);
    FormulaPtr skolemize(const FormulaPtr& formula, bool inPlace = false);
    static FormulaPtr eliminateQuantifiers(const FormulaPtr& formula,
        QuantificationFormula::Quantifier typeToRemove, bool inPlace = false);
    static JunctionFormulaPtr flattenToJunction(const FormulaPtr& formula,
        JunctionFormula::Operator targetOp, bool inPlace = false);

private:
    std::shared_ptr<NameRegistry> nameRegistry;

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
    static void collectOperandsRec(const FormulaPtr& formula,
        JunctionFormula::Operator targetOp, std::vector<FormulaPtr>& accumulator);
};

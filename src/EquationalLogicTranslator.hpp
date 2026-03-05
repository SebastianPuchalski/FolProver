#pragma once

#include "Expression.hpp"
#include "ProofNode.hpp"

#include <map>
#include <string>
#include <vector>

class EquationalLogicTranslator {
public:
    std::vector<ProofNodePtr> translateToEquationalLogic(
        const std::vector<ProofNodePtr>& clauseNodes) const;

    const std::string SYMBOL_TRUE = "$true";
    const std::string SYMBOL_FALSE = "$false";
    const std::string SYMBOL_OR = "$or";
    const std::string SYMBOL_NOT = "$not";
    const std::string SYMBOL_EQ = "$eq";

private:
    bool useExtractionRule = true;

    void extractSymbols(
        const std::vector<ProofNodePtr>& clauseNodes,
        std::map<std::string, size_t>& functionArities,
        std::map<std::string, size_t>& predicateArities,
        std::vector<std::string>& distinctObjects,
        bool& hasNot, bool& hasEquality) const;
    ProofNodePtr translateClause(const ProofNodePtr& clauseNode) const;
    TermPtr translateFormulaToTerm(const FormulaPtr& formula) const;
    std::vector<ProofNodePtr> generateBooleanAxioms() const;
    std::vector<ProofNodePtr> generateEqualityAxioms(
        const std::map<std::string, size_t>& functionArities,
        const std::map<std::string, size_t>& predicateArities) const;
    std::vector<ProofNodePtr> generateDistinctObjectAxioms(
        const std::vector<std::string>& distinctObjects) const;
    std::vector<ProofNodePtr> generateMetaRules() const;
    std::vector<ProofNodePtr> translateToDisjunction(
        const std::vector<ProofNodePtr>& nodes) const;
};

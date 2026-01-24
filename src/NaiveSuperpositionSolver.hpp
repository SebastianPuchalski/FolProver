#pragma once

#include "ExpressionTransformer.hpp"
#include "FolSatSolver.hpp"
#include "Lpo.hpp"

#include <deque>
#include <map>
#include <string>
#include <vector>

class NaiveSuperpositionSolver : public FolSatSolver {
public:
    void setTimeLimit(int seconds) override;
    void setMemoryLimit(int megabytes) override;

    Result solve(const std::vector<ProofNodePtr>& clauses) override;
    ProofNodePtr getProof() const override;

private:
    struct Clause;
    using ClausePtr = std::shared_ptr<Clause>;
    using Substitution = std::map<std::string, TermPtr>;

    ExpressionTransformer transformer;
    Lpo lpo;
    ClausePtr proofRoot;

    double timeLimitSeconds = 0.0;
    int memoryLimitMegabytes = 0;

    void applyBinaryResolution(const ClausePtr& clause1, const ClausePtr& clause2,
        std::vector<ClausePtr>& resolvents);
    void applyFactoring(const ClausePtr& clause, std::vector<ClausePtr>& factors);
    void applySuperposition(const ClausePtr& clause1, const ClausePtr& clause2,
        std::vector<ClausePtr>& inferredClauses);
    void applyEqualityResolution(const ClausePtr& clause,
        std::vector<ClausePtr>& inferredClauses);
    void applyEqualityFactoring(const ClausePtr& clause,
        std::vector<ClausePtr>& inferredClauses);

    bool removeBoolLiterals(std::vector<FormulaPtr>& literals);
    bool handleDistinctObjects(std::vector<FormulaPtr>& literals);
    void standardizeVariables(ClausePtr& clause);

    bool unify(const ExpressionPtr& expr1, const ExpressionPtr& expr2,
        Substitution& mgu);
    bool performOccursCheck(const std::string& symbol, const ExpressionPtr& expr,
        const Substitution& mgu);
    ExpressionPtr substitute(const ExpressionPtr& expr,
        const Substitution& substitution, bool inPlace = false);

    std::vector<bool> selectLiterals(
        const std::vector<FormulaPtr>& literals) const;
    std::vector<bool> areEligibleForResolution(
        const std::vector<FormulaPtr>& literals,
        const std::vector<bool>& selectionMask) const;
    std::vector<bool> areEligibleForParamodulation(
        const std::vector<FormulaPtr>& literals,
        const std::vector<bool>& selectionMask,
        bool strictlyMaximal = false) const;

    ProofNodePtr reconstructProof(const ClausePtr& clause,
        std::map<ClausePtr, ProofNodePtr>& cache) const;
};

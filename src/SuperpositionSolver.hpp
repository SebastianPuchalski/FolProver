#pragma once

#include "ExpressionTransformer.hpp"
#include "FolSatSolver.hpp"
#include "Lpo.hpp"

#include <map>
#include <vector>

class SuperpositionSolver : public FolSatSolver {
public:
    void setTimeLimit(int seconds) override;
    void setMemoryLimit(int megabytes) override;

    Result solve(const std::vector<ProofNodePtr>& clauses) override;
    ProofNodePtr getProof() const override;

private:
    struct Clause;
    using ClausePtr = std::shared_ptr<Clause>;
    using Clauses = std::vector<ClausePtr>;
    class ClauseSelector;
    class ClauseIndex;

    ExpressionTransformer transformer;
    Lpo lpo;
    ClausePtr proofRoot;

    double timeLimitSeconds = 0.0;
    int memoryLimitMegabytes = 0;

    bool loadInitialClauses(const std::vector<ProofNodePtr>& clauses, ClauseSelector& unprocessedClauses);
    ClausePtr simplifyForward(const ClausePtr& clause, ClauseIndex& index) const;
    ClausePtr simplifyCheapForward(const ClausePtr& clause, ClauseIndex& index) const;
    ClausePtr simplifyTrivialForward(const ClausePtr& clause, ClauseIndex& index) const;
    ClausePtr simplifyNecessaryForward(const ClausePtr& clause, ClauseIndex& index) const;
    Clauses simplifyBackward(ClauseIndex& index, const ClausePtr& givenClause) const;
    Clauses generateInferences(const ClausePtr& givenClause, ClauseIndex& index) const;

    void applyBinaryResolution(const ClausePtr& clause1, const ClausePtr& clause2, Clauses& resolvents) const;
    void applyFactoring(const ClausePtr& clause, Clauses& factors) const;
    void applySuperposition(const ClausePtr& clause1, const ClausePtr& clause2, Clauses& inferredClauses) const;
    void applyEqualityResolution(const ClausePtr& clause, Clauses& inferredClauses) const;
    void applyEqualityFactoring(const ClausePtr& clause, Clauses& inferredClauses) const;

    bool removeBoolLiterals(std::vector<FormulaPtr>& literals, bool* changed = nullptr) const;
    bool handleDistinctObjects(std::vector<FormulaPtr>& literals, bool* changed = nullptr) const;
    void standardizeVariables(ClausePtr& clause);

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

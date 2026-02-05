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
    ClausePtr simplifyForward(const ClausePtr& clauseToSimplify, const ClauseIndex& index) const;
    ClausePtr simplifyCheapForward(const ClausePtr& clauseToSimplify, const ClauseIndex& index) const;
    void simplifyBackward(ClauseIndex& indexToSimplify, const ClausePtr& clause, Clauses& reducedClauses) const;
    ClausePtr simplifyNecessary(const ClausePtr& clauseToSimplify) const;
    void generateInferences(const ClausePtr& clause, const ClauseIndex& index, Clauses& inferredClauses) const;

    void applyBinaryResolution(const ClausePtr& clause1, const ClausePtr& clause2, Clauses& resolvents) const;
    void applyFactoring(const ClausePtr& clause, Clauses& factors) const;
    void applySuperposition(const ClausePtr& clause1, const ClausePtr& clause2, Clauses& paramodulants) const;
    void applyEqualityResolution(const ClausePtr& clause, Clauses& inferredClauses) const;
    void applyEqualityFactoring(const ClausePtr& clause, Clauses& inferredClauses) const;

    ClausePtr applyTautologyDeletion(const ClausePtr& clause) const;             // (TD)
    ClausePtr applyDeletionOfDuplicateLiterals(const ClausePtr& clause) const;   // (DD)
    ClausePtr applyDeletionOfResolvedLiterals(const ClausePtr& clause) const;    // (DR)
    ClausePtr applyDestructiveEqualityResolution(const ClausePtr& clause) const; // (DE)
    ClausePtr applyPredicateUnitSimplification(const ClausePtr& clause, const ClausePtr& unitClause) const;
    ClausePtr applyDemodulation(const ClausePtr& clause, const ClausePtr& unitClause) const; // RN, RP
    ClausePtr applyClauseSubsumption(const ClausePtr& subsumed, const ClausePtr& subsuming) const; // CS
    ClausePtr applyEqualitySubsumption(const ClausePtr& subsumed, const ClausePtr& unitClause) const; // ES
    ClausePtr applyPositiveSimplifyReflect(const ClausePtr& clause, const ClausePtr& unitClause) const; // PS
    ClausePtr applyNegativeSimplifyReflect(const ClausePtr& clause, const ClausePtr& unitClause) const; // NS

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

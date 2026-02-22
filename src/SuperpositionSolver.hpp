#pragma once

#include "ExpressionTransformer.hpp"
#include "FolSatSolver.hpp"
#include "Lpo.hpp"
#include "SuperpositionSolverUtils.hpp"

#include <map>
#include <vector>

class SuperpositionSolver : public FolSatSolver {
public:
    SuperpositionSolver();

    void setTimeLimit(int seconds) override;
    void setMemoryLimit(int megabytes) override;

    void setAnswerPredicateName(const std::string& name) override;

    Result solve(const std::vector<ProofNodePtr>& clauses) override;
    ProofNodePtr getProof() const override;

private:
    using Literals = SuperpositionSolverUtils::Literals;
    using LiteralSelector = SuperpositionSolverUtils::LiteralSelector;
    using Clause = SuperpositionSolverUtils::Clause;
    using ClausePtr = SuperpositionSolverUtils::ClausePtr;
    using Clauses = SuperpositionSolverUtils::Clauses;
    using ClauseSelector = SuperpositionSolverUtils::ClauseSelector;
    using ClauseIndex = SuperpositionSolverUtils::ClauseIndex;

    Lpo lpo;
    LiteralSelector literalSelector;
    ExpressionTransformer transformer;

    double timeLimitSeconds = 0.0;
    int memoryLimitMegabytes = 0;

    std::string answerPredicateName;
    ClausePtr proofRoot;

    std::pair<double, size_t> initResourceLimitState() const;
    Result checkResourceLimits(std::pair<double, size_t>& state) const;
    ClauseSelector createClauseSelector() const;
    bool loadInitialClauses(const std::vector<ProofNodePtr>& clauses, ClauseSelector& unprocessedClauses);
    ClausePtr simplifyForward(const ClausePtr& clauseToSimplify, const ClauseIndex& index) const;
    ClausePtr simplifyCheapForward(const ClausePtr& clauseToSimplify, const ClauseIndex& index) const;
    void simplifyBackward(ClauseIndex& indexToSimplify, const ClausePtr& clause,
        Clauses& reducedClauses, ClauseSelector& unprocessedClauses) const;
    void generateInferences(const ClausePtr& clause, const ClauseIndex& index, Clauses& inferredClauses) const;
    void makeClauseVariablesUnique(ClausePtr& clause);
    bool satisfiesStopCondition(const ClausePtr& clause);

    ClausePtr applyBooleanSimplification(const ClausePtr& clause) const;
    ClausePtr applyDistinctObjectSimplification(const ClausePtr& clause) const;

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
    ClausePtr applyDemodulation(const ClausePtr& clause, const ClausePtr& unitClause) const;            // RN, RP
    ClausePtr applyClauseSubsumption(const ClausePtr& subsumed, const ClausePtr& subsuming) const;      // CS
    ClausePtr applyEqualitySubsumption(const ClausePtr& subsumed, const ClausePtr& unitClause) const;   // ES
    ClausePtr applyPositiveSimplifyReflect(const ClausePtr& clause, const ClausePtr& unitClause) const; // PS
    ClausePtr applyNegativeSimplifyReflect(const ClausePtr& clause, const ClausePtr& unitClause) const; // NS

    ProofNodePtr reconstructProof(const ClausePtr& clause,
        std::map<ClausePtr, ProofNodePtr>& cache) const;
};

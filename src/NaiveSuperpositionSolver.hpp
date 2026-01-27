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
        std::vector<ClausePtr>& resolvents) const;
    void applyFactoring(const ClausePtr& clause, std::vector<ClausePtr>& factors) const;
    void applySuperposition(const ClausePtr& clause1, const ClausePtr& clause2,
        std::vector<ClausePtr>& inferredClauses) const;
    void applyEqualityResolution(const ClausePtr& clause,
        std::vector<ClausePtr>& inferredClauses) const;
    void applyEqualityFactoring(const ClausePtr& clause,
        std::vector<ClausePtr>& inferredClauses) const;

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

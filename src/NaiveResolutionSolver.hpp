#pragma once

#include "FolSatSolver.hpp"
#include "ExpressionTransformer.hpp"

#include <deque>
#include <map>
#include <string>
#include <vector>

class NaiveResolutionSolver : public FolSatSolver {
public:
    void setTimeLimit(int seconds) override;
    void setMemoryLimit(int megabytes) override;

    void setAnswerPredicateSymbol(const std::string& name) override;

    Result solve(const std::vector<ProofNodePtr>& clauses) override;
    ProofNodePtr getProof() const override;

private:
    struct Clause;
    using ClausePtr = std::shared_ptr<Clause>;

    ExpressionTransformer transformer;
    ClausePtr proofRoot;

    double timeLimitSeconds = 0.0;
    int memoryLimitMegabytes = 0;

    void resolve(const ClausePtr& clause1, const ClausePtr& clause2,
        std::vector<ClausePtr>& resolvents) const;
    void factor(const ClausePtr& clause, std::vector<ClausePtr>& factors) const;
    void addEqualityAxioms(std::deque<ClausePtr>& clauses);

    bool removeBoolLiterals(std::vector<FormulaPtr>& literals) const;
    bool handleDistinctObjects(std::vector<FormulaPtr>& literals) const;
    void standardizeVariables(ClausePtr& clause);

    ProofNodePtr reconstructProof(const ClausePtr& clause,
        std::map<ClausePtr, ProofNodePtr>& cache) const;
};

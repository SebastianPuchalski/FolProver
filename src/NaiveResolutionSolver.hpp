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

    Result solve(const std::vector<ProofNodePtr>& clauses) override;
    ProofNodePtr getProof() const override;

private:
    struct Clause;
    using ClausePtr = std::shared_ptr<Clause>;
    using Substitution = std::map<std::string, TermPtr>;

    ExpressionTransformer transformer;
    ClausePtr proofRoot;

    double timeLimitSeconds = 0.0;
    int memoryLimitMegabytes = 0;

    bool removeBoolLiterals(std::vector<FormulaPtr>& literals);
    void standardizeVariables(ClausePtr& clause);
    void resolve(const ClausePtr& clause1, const ClausePtr& clause2,
        std::vector<ClausePtr>& resolvents);
    void factor(const ClausePtr& clause, std::vector<ClausePtr>& factors);
    bool unify(const ExpressionPtr& expr1, const ExpressionPtr& expr2,
        Substitution& mgu);
    bool occursCheck(const std::string& symbol, const ExpressionPtr& expr,
        const Substitution& mgu);
    ExpressionPtr substitute(const ExpressionPtr& expr,
        const Substitution& substitution, bool inPlace = false);
    void addEqualityAxioms(std::deque<ClausePtr>& clauses);
    ProofNodePtr reconstructProof(const ClausePtr& clause,
        std::map<ClausePtr, ProofNodePtr>& cache) const;
    bool handleDistinctObjects(std::vector<FormulaPtr>& literals);
};

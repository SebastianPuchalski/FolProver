#pragma once

#include "Expression.hpp"
#include "ProofNode.hpp"

#include <vector>

class FolSatSolver {
public:
    enum class Result {
        UNSATISFIABLE,
        SATISFIABLE,
        TIME_OUT,
        MEMORY_OUT,
        UNKNOWN
    };

    virtual ~FolSatSolver() = default;

    virtual void setTimeLimit(int seconds) = 0;
    virtual void setMemoryLimit(int megabytes) = 0;

    virtual Result solve(const std::vector<ProofNodePtr>& formulas) = 0;
    virtual ProofNodePtr getProof() const = 0;
};

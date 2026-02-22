#pragma once

#include "../FolSatSolver.hpp"
#include "Loader.hpp"

#include <string>

namespace TptpTool {

class Solver {
public:
    enum class OutStatus {
        // Logical Validity (Problems with Conjecture)
        THEOREM,             // Axioms entail conjecture (Axioms |= Conjecture)
        COUNTER_SATISFIABLE, // Axioms do not entail conjecture (Counter-example found)

        // Consistency/Satisfiability (Problems without Conjecture)
        UNSATISFIABLE,       // The set of formulae is unsatisfiable
        SATISFIABLE,         // The set of formulae is satisfiable

        // Resource Limits
        TIME_OUT,            // CPU time limit exceeded
        MEMORY_OUT,          // Memory limit exceeded

        // Errors
        INPUT_ERROR,         // User input parameters or syntax error
        OS_ERROR,            // Operating system or I/O error
        ERROR,               // Internal software error or unhandled exception

        // Fallback
        UNKNOWN              // Result could not be determined
    };

    explicit Solver(std::string inFilePath, std::string tptpDir, bool prepareProof = false);

    std::string getTextProof() const;
    std::string getTstpProof() const;
    std::string getHtmlProof() const;
    OutStatus solve(int timeLimitSeconds = -1,
                    int memoryLimitMegabytes = -1,
                    const std::string& answerPredicate = "");

private:
    const std::string inFilePath;
    const std::string tptpDir;
    const bool prepareProof;

    std::string textProof;
    std::string tstpProof;
    std::string htmlProof;

    struct ProblemDef {
        std::vector<ProofNodePtr> formulaNodes;
        bool isRefutation;
    };

    ProblemDef createProblemDef(std::vector<Loader::AnnotatedFormula> annotatedFormulas) const;
    std::vector<ProofNodePtr> convertToCnf(const std::vector<ProofNodePtr>& nodes) const;
};

} // namespace TptpTool

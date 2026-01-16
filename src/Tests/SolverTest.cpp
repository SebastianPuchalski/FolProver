#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../TptpTool/Solver.hpp" 

namespace fs = std::filesystem;
using namespace TptpTool;

class SolverTest : public ::testing::Test {
protected:
    std::string tptpDir;
    std::vector<std::string> tempFiles;

    void SetUp() override {
        const char* envName = "TPTP_DIR";
        const char* envVal = std::getenv(envName);
        if (!envVal) FAIL() << "Environment variable TPTP_DIR is not set.";

        tptpDir = envVal;
        if (!fs::exists(tptpDir)) FAIL() << "TPTP_DIR does not exist: " << tptpDir;
    }

    void TearDown() override {
        for (const auto& file : tempFiles) {
            if (fs::exists(file)) {
                std::error_code ec;
                fs::remove(file, ec);
            }
        }
    }

    std::string createProblemFile(const std::string& filename, const std::string& content) {
        std::ofstream tmp(filename);
        tmp << content;
        tmp.close();
        tempFiles.push_back(filename);
        return filename;
    }

    void solveAndCheck(const std::string& path,
        Solver::OutStatus expectedStatus,
        bool expectProof = false,
        bool useTptpDirPrefix = true,
        int timeLimit = -1,
        int memoryLimit = -1) {

        std::string fullPathString = path;

        if (useTptpDirPrefix) {
            fs::path p = fs::path(tptpDir) / path;
            fullPathString = p.string();
            if (!fs::exists(p)) FAIL() << "TPTP file missing: " << fullPathString;
        }

        Solver solver(fullPathString, tptpDir, expectProof);
        Solver::OutStatus result = solver.solve(timeLimit, memoryLimit);

        auto statusToString = [](Solver::OutStatus status) -> std::string {
            switch (status) {
            case Solver::OutStatus::THEOREM:             return "THEOREM";
            case Solver::OutStatus::COUNTER_SATISFIABLE: return "COUNTER_SATISFIABLE";
            case Solver::OutStatus::UNSATISFIABLE:       return "UNSATISFIABLE";
            case Solver::OutStatus::SATISFIABLE:         return "SATISFIABLE";
            case Solver::OutStatus::TIME_OUT:            return "TIME_OUT";
            case Solver::OutStatus::MEMORY_OUT:          return "MEMORY_OUT";
            case Solver::OutStatus::INPUT_ERROR:         return "INPUT_ERROR";
            case Solver::OutStatus::OS_ERROR:            return "OS_ERROR";
            case Solver::OutStatus::ERROR:               return "ERROR";
            case Solver::OutStatus::UNKNOWN:             return "UNKNOWN";
            default:                                     return "UNKNOWN";
            }
            };

        EXPECT_EQ(result, expectedStatus)
            << "File: " << path
            << "\nExpected: " << statusToString(expectedStatus)
            << "\nActual:   " << statusToString(result);

        if (expectProof && result == Solver::OutStatus::THEOREM) {
            EXPECT_FALSE(solver.getTstpProof().empty()) << "Proof output is empty.";
        }
    }
};

// =========================================================================
// GROUP 1: STANDARD TPTP BENCHMARKS
// =========================================================================

TEST_F(SolverTest, TPTP_Syn001_1_Syntactic) {
    solveAndCheck("Problems/SYN/SYN001+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Puz001_Minus_1_Agatha_CNF) {
#ifndef NDEBUG
    std::cout << "[ INFO     ] Skipping slow test in Debug mode (TPTP_Puz001_Minus_1_Agatha_CNF)." << std::endl;
    return;
#else
    solveAndCheck("Problems/PUZ/PUZ001-1.p", Solver::OutStatus::UNSATISFIABLE);
#endif
}

// =========================================================================
// GROUP 2: EQUALITY MICRO-BENCHMARKS (FAST)
// =========================================================================

TEST_F(SolverTest, TPTP_Puz001_1_Agatha_FOF) {
#ifndef NDEBUG
    std::cout << "[ INFO     ] Skipping slow test in Debug mode (TPTP_Puz001_1_Agatha_FOF)." << std::endl;
    return;
#else
    solveAndCheck("Problems/PUZ/PUZ001+1.p", Solver::OutStatus::THEOREM, false, true, 10);
#endif
}

// Test 1: Reflexivity (X = X).
// Simple proof: ~(a = a) should be UNSAT immediately.
TEST_F(SolverTest, Equality_Reflexivity) {
    std::string filename = createProblemFile("eq_reflexivity.p", R"(
        fof(conjecture, conjecture, a = a).
    )");
    solveAndCheck(filename, Solver::OutStatus::THEOREM, true, false);
}

// Test 2: Symmetry (X = Y => Y = X).
// Given a = b, prove b = a.
TEST_F(SolverTest, Equality_Symmetry) {
    std::string filename = createProblemFile("eq_symmetry.p", R"(
        fof(axiom, axiom, a = b).
        fof(conjecture, conjecture, b = a).
    )");
    solveAndCheck(filename, Solver::OutStatus::THEOREM, true, false);
}

// Test 3: Transitivity (X = Y & Y = Z => X = Z).
// Given a = b and b = c, prove a = c.
TEST_F(SolverTest, Equality_Transitivity) {
    std::string filename = createProblemFile("eq_transitivity.p", R"(
        fof(ax1, axiom, a = b).
        fof(ax2, axiom, b = c).
        fof(conjecture, conjecture, a = c).
    )");
    solveAndCheck(filename, Solver::OutStatus::THEOREM, true, false);
}

// Test 4: Function Congruence (X = Y => f(X) = f(Y)).
// Given a = b, prove f(a) = f(b).
TEST_F(SolverTest, Equality_FunctionCongruence) {
    std::string filename = createProblemFile("eq_func_congruence.p", R"(
        fof(ax1, axiom, a = b).
        fof(conjecture, conjecture, f(a) = f(b)).
    )");
    solveAndCheck(filename, Solver::OutStatus::THEOREM, true, false);
}

// Test 5: Predicate Congruence (X = Y & P(X) => P(Y)).
// Given a = b and P(a) is true, prove P(b) is true.
TEST_F(SolverTest, Equality_PredicateCongruence) {
#ifndef NDEBUG
    std::cout << "[ INFO     ] Skipping slow test in Debug mode (Equality_PredicateCongruence)." << std::endl;
    return;
#else
    std::string filename = createProblemFile("eq_pred_congruence.p", R"(
        fof(ax1, axiom, a = b).
        fof(ax2, axiom, p(a)).
        fof(conjecture, conjecture, p(b)).
    )");
    solveAndCheck(filename, Solver::OutStatus::THEOREM, true, false);
#endif
}

// Test 6: Nested Functions (Complex Congruence).
// Given f(a) = b and g(b) = c, prove g(f(a)) = c.
TEST_F(SolverTest, Equality_NestedFunctions) {
    std::string filename = createProblemFile("eq_nested.p", R"(
        fof(ax1, axiom, f(a) = b).
        fof(ax2, axiom, g(b) = c).
        fof(conjecture, conjecture, g(f(a)) = c).
    )");
    solveAndCheck(filename, Solver::OutStatus::THEOREM, true, false);
}

// Test 7: Negative Test (Should NOT prove).
// Given a = b, prove a = c. This is false.
TEST_F(SolverTest, Equality_InvalidInference) {
#ifndef NDEBUG
    std::cout << "[ INFO     ] Skipping slow test in Debug mode (Equality_InvalidInference)." << std::endl;
    return;
#else
    std::string filename = createProblemFile("eq_invalid.p", R"(
        fof(ax1, axiom, a = b).
        fof(conjecture, conjecture, a = c).
    )");
    solveAndCheck(filename, Solver::OutStatus::COUNTER_SATISFIABLE, false, false, 4);
#endif
}

// Test 8: Arity Check (Multi-argument function).
// Given a=x and b=y, prove h(a,b) = h(x,y).
TEST_F(SolverTest, Equality_MultiArgFunction) {
#ifndef NDEBUG
    std::cout << "[ INFO     ] Skipping slow test in Debug mode (Equality_MultiArgFunction)." << std::endl;
    return;
#else
    std::string filename = createProblemFile("eq_multi_arg.p", R"(
        fof(ax1, axiom, a = x).
        fof(ax2, axiom, b = y).
        fof(conjecture, conjecture, h(a,b) = h(x,y)).
    )");
    solveAndCheck(filename, Solver::OutStatus::THEOREM, true, false);
#endif
}

// =========================================================================
// GROUP 3: INTEGRATION (TPTP ROLES & FORMULATION)
// =========================================================================

TEST_F(SolverTest, Integration_RoleDistinction_AxiomsOnly) {
    // Two conflicting axioms: p and ~p.
    // Should be UNSATISFIABLE (consistent set impossible), NOT THEOREM.
    std::string content = "fof(a1, axiom, p). fof(a2, axiom, ~p).";
    std::string file = createProblemFile("test_role_distinction.p", content);
    solveAndCheck(file, Solver::OutStatus::UNSATISFIABLE, false, false);
}

TEST_F(SolverTest, Integration_ExFalsoQuodlibet) {
    // Contradictory axioms ({p, ~p}) allow proving any conjecture (q).
    std::string content = "fof(a1, axiom, p). fof(a2, axiom, ~p). fof(c, conjecture, q).";
    std::string file = createProblemFile("test_ex_falso.p", content);
    solveAndCheck(file, Solver::OutStatus::THEOREM, false, false);
}

TEST_F(SolverTest, Integration_MixedRoles_StandardCompliance) {
    // Standard TPTP mix:
    // Axiom: p | q
    // Negated Conjecture: ~p (treated as axiom)
    // Conjecture: q (negated by solver -> ~q)
    // Result: {p|q, ~p, ~q} -> Contradiction -> THEOREM.
    std::string content = R"(
        fof(ax, axiom, p | q).
        fof(nc, negated_conjecture, ~p).
        fof(co, conjecture, q).
    )";
    std::string file = createProblemFile("test_mixed_roles.p", content);
    solveAndCheck(file, Solver::OutStatus::THEOREM, false, false);
}

// =========================================================================
// GROUP 4: CUSTOM LOGIC & EDGE CASES (RESTORED)
// =========================================================================

TEST_F(SolverTest, Logic_VariableShadowing) {
    // Scope test: ![X]:p(X) vs ?[X]:p(X).
    std::string content = "fof(ax, axiom, ![X]: p(X)). fof(conj, conjecture, ?[X]: p(X)).";
    std::string file = createProblemFile("test_shadowing.p", content);
    solveAndCheck(file, Solver::OutStatus::THEOREM, false, false);
}

TEST_F(SolverTest, Logic_Function_Depth) {
    // Recursion test: p(a) -> p(f(a)) -> p(f(f(a))).
    std::string content = "fof(b,axiom,p(a)). fof(s,axiom,![X]:(p(X)=>p(f(X)))). fof(t,conjecture,p(f(f(a)))).";
    std::string file = createProblemFile("test_func_depth.p", content);
    solveAndCheck(file, Solver::OutStatus::THEOREM, false, false);
}

TEST_F(SolverTest, Logic_Distraction_Axioms) {
    // Needle in a haystack.
    std::string content = "fof(j1,axiom,a=>b). fof(r,axiom,p). fof(g,conjecture,p).";
    std::string file = createProblemFile("test_distraction.p", content);
    solveAndCheck(file, Solver::OutStatus::THEOREM, false, false);
}

TEST_F(SolverTest, Logic_DrinkerParadox) {
    // ?[X]: (d(X) => ![Y]: d(Y)).
    std::string content = "fof(d, conjecture, ?[X] : (d(X) => ![Y] : d(Y))).";
    std::string file = createProblemFile("test_drinker.p", content);
    solveAndCheck(file, Solver::OutStatus::THEOREM, false, false);
}

TEST_F(SolverTest, Logic_Transitivity_Chain) {
    // a -> b -> c -> d.
    std::string content = "fof(1,axiom,a). fof(2,axiom,a=>b). fof(3,axiom,b=>c). fof(g,conjecture,c).";
    std::string file = createProblemFile("test_chain.p", content);
    solveAndCheck(file, Solver::OutStatus::THEOREM, false, false);
}

TEST_F(SolverTest, Logic_CounterSatisfiable_Swap) {
    // Quantifier swap invalidity.
    std::string content = "fof(ax, axiom, ![X]:?[Y]: l(X,Y)). fof(c, conjecture, ?[Y]:![X]: l(X,Y)).";
    std::string file = createProblemFile("test_quant_swap.p", content);
    solveAndCheck(file, Solver::OutStatus::COUNTER_SATISFIABLE, false, false);
}

TEST_F(SolverTest, Logic_Satisfiable_Simple) {
    // Consistency check (SAT).
    std::string content = "fof(a1, axiom, p(a)). fof(a2, axiom, q(b)).";
    std::string file = createProblemFile("test_sat.p", content);
    solveAndCheck(file, Solver::OutStatus::SATISFIABLE, false, false);
}

TEST_F(SolverTest, Logic_DoubleNegation) {
    // ~~p <=> p.
    std::string content = "fof(goal, conjecture, ~~p <=> p).";
    std::string file = createProblemFile("test_double_neg.p", content);
    solveAndCheck(file, Solver::OutStatus::THEOREM, false, false);
}

// =========================================================================
// GROUP 5: ERROR HANDLING
// =========================================================================

TEST_F(SolverTest, Error_FileNotFound) {
    fs::path p = fs::path(tptpDir) / "Problems/XYZ/Missing.p";
    Solver solver(p.string(), tptpDir);
    EXPECT_EQ(solver.solve(), Solver::OutStatus::INPUT_ERROR);
}

TEST_F(SolverTest, Error_InvalidSyntax) {
    std::string content = "fof(broken, axiom, (p | q .";
    std::string file = createProblemFile("invalid_syntax.p", content);
    Solver solver(file, tptpDir);
    EXPECT_EQ(solver.solve(), Solver::OutStatus::INPUT_ERROR);
}

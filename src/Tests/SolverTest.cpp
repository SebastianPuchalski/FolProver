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

TEST_F(SolverTest, TPTP_Syn919_1_Smullyan) {
    solveAndCheck("Problems/SYN/SYN919+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Syn009_Minus_1_Relevancy) {
    solveAndCheck("Problems/SYN/SYN009-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Syn077_Minus_1_Pelletier54) {
#ifndef NDEBUG
    std::cout << "[ INFO      ] Skipping slow test in Debug mode (TPTP_Syn077_Minus_1_Pelletier54)." << std::endl;
    return;
#else
    solveAndCheck("Problems/SYN/SYN077-1.p", Solver::OutStatus::UNSATISFIABLE);
#endif
}

TEST_F(SolverTest, TPTP_Syn094_1_005_Plaisted) {
    solveAndCheck("Problems/SYN/SYN094-1.005.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Grp001_1_X_Squared_Is_Identity) {
    solveAndCheck("Problems/GRP/GRP001-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Grp048_10_InverseSubstitution) {
#ifndef NDEBUG
    std::cout << "[ INFO      ] Skipping slow test in Debug mode (TPTP_Grp048_10_InverseSubstitution)." << std::endl;
    return;
#else
    solveAndCheck("Problems/GRP/GRP048-10.p", Solver::OutStatus::UNSATISFIABLE);
#endif
    // [       OK ] SolverTest.TPTP_Grp048_10_InverseSubstitution (4422505 ms)
}

TEST_F(SolverTest, DISABLED_TPTP_Grp049_1_SingleAxiomGroup) {
    solveAndCheck("Problems/GRP/GRP049-1.p", Solver::OutStatus::UNSATISFIABLE);
}

// =========================================================================
// GROUP 2: EQUALITY MICRO-BENCHMARKS (FAST)
// =========================================================================

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
// GROUP 5: SELF-SUPERPOSITION
// =========================================================================

TEST_F(SolverTest, Superposition_Mandatory_SelfOverlap) {
    // This test case STRICTLY requires self-superposition to find the proof.
    // Axiom: f(f(X)) = a
    // Goal: f(a) = a
    // Without self-superposition, the solver cannot bridge the gap
    // because the goal literal f(a) does not contain the redex f(f(X)).

    std::string filename = createProblemFile("mandatory_self.p", R"(
        cnf(ax1, axiom, f(f(X)) = a).
        cnf(conj, negated_conjecture, f(a) != a).
    )");

    // Should return UNSATISFIABLE.
    // If it returns SATISFIABLE, the self-superposition logic is broken or missing.
    solveAndCheck(filename, Solver::OutStatus::UNSATISFIABLE, false, false, 2);
}

TEST_F(SolverTest, Resolution_Mandatory_SelfBinary) {
    // Tests if the prover can derive R = P(x) | ~P(f(f(x)))
    // from C = P(x) | ~P(f(x)) using self-resolution.
    std::string filename = createProblemFile("self_resolution.p", R"(
        fof(ax1, axiom, ![X]: (p(X) | ~p(f(X)))).
        fof(goal, conjecture, ![X]: (p(X) | ~p(f(f(X))))).
    )");

    solveAndCheck(filename, Solver::OutStatus::THEOREM, false, false, 2);
}

TEST_F(SolverTest, DISABLED_Superposition_Intermediate_ImplicitIdentity) {
    // AXIOMS:
    // 1. Associativity: (x * y) * z = x * (y * z)
    // 2. Left Cancellation/Inverse property: x * (inv(x) * y) = y
    //
    // NOTE: We do NOT define 'e'. The solver implies it.
    //
    // GOAL:
    // Prove that x * inv(x) is a constant (i.e., a * inv(a) == b * inv(b)).
    // This requires deriving that x * inv(x) behaves like a right identity.

    std::string filename = createProblemFile("implicit_id.p", R"(
        cnf(assoc, axiom, multiply(multiply(X,Y),Z) = multiply(X,multiply(Y,Z))).
        cnf(l_cancel, axiom, multiply(X, multiply(inverse(X), Y)) = Y).

        cnf(prove_id_constant, negated_conjecture,
            multiply(a, inverse(a)) != multiply(b, inverse(b))).
    )");

    // Should solve in < 2-4 seconds.
    // If this hangs, your solver fails to synthesize constants (lemmas)
    // from variable-only axioms.
    solveAndCheck(filename, Solver::OutStatus::UNSATISFIABLE, false, false);
}

// =========================================================================
// GROUP 6: ERROR HANDLING
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

// =========================================================================
// GROUP 1: PUZ TPTP BENCHMARKS
// =========================================================================

TEST_F(SolverTest, TPTP_Puz001_Minus_3) {
    solveAndCheck("Problems/PUZ/PUZ001-3.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz002_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ002-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz003_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ003-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz004_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ004-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz005_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ005+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Puz005_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ005-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz001_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ001+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Puz001_Plus_2) {
    solveAndCheck("Problems/PUZ/PUZ001+2.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Puz001_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ001-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz001_Minus_2) {
    solveAndCheck("Problems/PUZ/PUZ001-2.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz006_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ006-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz007_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ007-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz008_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ008-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz008_Minus_2) {
    solveAndCheck("Problems/PUZ/PUZ008-2.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz008_Minus_3) {
#ifndef NDEBUG
    std::cout << "[ INFO      ] Skipping slow test in Debug mode (TPTP_Puz008_Minus_3)." << std::endl;
    return;
#else
    solveAndCheck("Problems/PUZ/PUZ008-3.p", Solver::OutStatus::UNSATISFIABLE);
#endif
}

TEST_F(SolverTest, TPTP_Puz009_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ009-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz010_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ010-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz011_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ011-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz012_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ012-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz013_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ013-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz014_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ014-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz015_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ015-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz015_Minus_2_006) {
    // [       OK ] SolverTest.TPTP_Puz015_Minus_2_006 (5873 ms)
    solveAndCheck("Problems/PUZ/PUZ015-2.006.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz015_Minus_3) {
    solveAndCheck("Problems/PUZ/PUZ015-3.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz015_Minus_10) {
    solveAndCheck("Problems/PUZ/PUZ015-10.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz016_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ016-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz016_Minus_2_004) {
    solveAndCheck("Problems/PUZ/PUZ016-2.004.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz016_Minus_2_005) {
    solveAndCheck("Problems/PUZ/PUZ016-2.005.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz017_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ017-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz018_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ018-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz018_Minus_2) {
    // Too slow after strictlyMaximal = false (getEligibleForParamodulationMask(literalSelector, false)
    solveAndCheck("Problems/PUZ/PUZ018-2.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz019_Minus_1) {
    // Too slow after strictlyMaximal = false (getEligibleForParamodulationMask(literalSelector, false)
#ifndef NDEBUG
    std::cout << "[ INFO      ] Skipping slow test in Debug mode (TPTP_Puz019_Minus_1)." << std::endl;
    return;
#else
    solveAndCheck("Problems/PUZ/PUZ019-1.p", Solver::OutStatus::UNSATISFIABLE);
#endif
}

TEST_F(SolverTest, TPTP_Puz020_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ020-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz021_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ021-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz022_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ022-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz023_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ023-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz024_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ024-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz025_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ025-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz026_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ026-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz027_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ027-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz028_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ028-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz028_Minus_2) {
    solveAndCheck("Problems/PUZ/PUZ028-2.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz028_Minus_3) {
    solveAndCheck("Problems/PUZ/PUZ028-3.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz028_Minus_4) {
    solveAndCheck("Problems/PUZ/PUZ028-4.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz028_Minus_5) {
    solveAndCheck("Problems/PUZ/PUZ028-5.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz028_Minus_6) {
    solveAndCheck("Problems/PUZ/PUZ028-6.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz029_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ029-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz030_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ030-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz030_Minus_2) {
    solveAndCheck("Problems/PUZ/PUZ030-2.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz031_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ031+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Puz031_Plus_2) {
    solveAndCheck("Problems/PUZ/PUZ031+2.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Puz031_Plus_3) {
    solveAndCheck("Problems/PUZ/PUZ031+3.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Puz031_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ031-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz032_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ032-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz033_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ033-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz034_Minus_1_003) {
    solveAndCheck("Problems/PUZ/PUZ034-1.003.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz034_Minus_1_004) {
    solveAndCheck("Problems/PUZ/PUZ034-1.004.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz035_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ035-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz035_Minus_2) {
    solveAndCheck("Problems/PUZ/PUZ035-2.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz035_Minus_3) {
    solveAndCheck("Problems/PUZ/PUZ035-3.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz035_Minus_4) {
    solveAndCheck("Problems/PUZ/PUZ035-4.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz035_Minus_5) {
    solveAndCheck("Problems/PUZ/PUZ035-5.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz035_Minus_6) {
    solveAndCheck("Problems/PUZ/PUZ035-6.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz035_Minus_7) {
    solveAndCheck("Problems/PUZ/PUZ035-7.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz036_1_005) {
    solveAndCheck("Problems/PUZ/PUZ036-1.005.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz037_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ037-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz037_Minus_2) {
    solveAndCheck("Problems/PUZ/PUZ037-2.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz037_Minus_3) {
    solveAndCheck("Problems/PUZ/PUZ037-3.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz038_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ038-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz039_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ039-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz039_Minus_10) {
    solveAndCheck("Problems/PUZ/PUZ039-10.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz040_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ040-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz041_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ041-1.p", Solver::OutStatus::UNKNOWN);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz042_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ042-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz043_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ043-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz044_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ044-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz045_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ045-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz046_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ046-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz047_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ047+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Puz047_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ047-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz048_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ048-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz049_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ049-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz049_Minus_10) {
    solveAndCheck("Problems/PUZ/PUZ049-10.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz050_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ050-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz051_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ051-1.p", Solver::OutStatus::UNKNOWN);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz051_Minus_10) {
    solveAndCheck("Problems/PUZ/PUZ051-10.p", Solver::OutStatus::UNKNOWN);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz052_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ052-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz053_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ053-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz054_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ054-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz054_Minus_10) {
    solveAndCheck("Problems/PUZ/PUZ054-10.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz055_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ055-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ056-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz056_Minus_2_005) {
    solveAndCheck("Problems/PUZ/PUZ056-2.005.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_2_010) {
    solveAndCheck("Problems/PUZ/PUZ056-2.010.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_2_015) {
    solveAndCheck("Problems/PUZ/PUZ056-2.015.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_2_020) {
    solveAndCheck("Problems/PUZ/PUZ056-2.020.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_2_022) {
    solveAndCheck("Problems/PUZ/PUZ056-2.022.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_2_025) {
    solveAndCheck("Problems/PUZ/PUZ056-2.025.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_2_027) {
    solveAndCheck("Problems/PUZ/PUZ056-2.027.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_2_030) {
    solveAndCheck("Problems/PUZ/PUZ056-2.030.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_10_020) {
    solveAndCheck("Problems/PUZ/PUZ056-10.020.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz056_Minus_10_030) {
    solveAndCheck("Problems/PUZ/PUZ056-10.030.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz057_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ057-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz058_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ058-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz059_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ059-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz060_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ060+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, TPTP_Puz061_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ061+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz062_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ062-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz062_Minus_2) {
    solveAndCheck("Problems/PUZ/PUZ062-2.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz063_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ063-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz063_Minus_2) {
    solveAndCheck("Problems/PUZ/PUZ063-2.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz064_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ064-1.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz064_Minus_2) {
    solveAndCheck("Problems/PUZ/PUZ064-2.p", Solver::OutStatus::UNSATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz065_Plus_1) {
    // [       OK] SolverTest.TPTP_Puz065_Plus_1(12656 ms)
    solveAndCheck("Problems/PUZ/PUZ065+1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz066_Plus_1) {
    // [       OK ] SolverTest.TPTP_Puz066_Plus_1 (9932 ms)
    solveAndCheck("Problems/PUZ/PUZ066+1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz067_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ067+1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, TPTP_Puz068_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ068+1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz068_Plus_2) {
    // [       OK ] SolverTest.TPTP_Puz068_Plus_2 (8412 ms)
    solveAndCheck("Problems/PUZ/PUZ068+2.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz068_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ068-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz069_Plus_1) {
    // [       OK ] SolverTest.TPTP_Puz069_Plus_1 (3808 ms)
    solveAndCheck("Problems/PUZ/PUZ069+1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz069_Plus_2) {
    // [       OK ] SolverTest.TPTP_Puz069_Plus_2 (9264 ms)
    solveAndCheck("Problems/PUZ/PUZ069+2.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz069_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ069-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz070_Plus_1) {
    // [       OK ] SolverTest.TPTP_Puz070_Plus_1 (3136 ms)
    solveAndCheck("Problems/PUZ/PUZ070+1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz070_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ070-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz071_Plus_1) {
    // [       OK ] SolverTest.TPTP_Puz071_Plus_1 (3539 ms)
    solveAndCheck("Problems/PUZ/PUZ071+1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz071_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ071-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz072_Plus_1) {
    // [       OK ] SolverTest.TPTP_Puz072_Plus_1 (12515 ms)
    solveAndCheck("Problems/PUZ/PUZ072+1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz072_Minus_1) {
    solveAndCheck("Problems/PUZ/PUZ072-1.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz073_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ073+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz074_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ074+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz075_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ075+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz076_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ076+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz077_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ077+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz078_Plus_1) {
    solveAndCheck("Problems/PUZ/PUZ078+1.p", Solver::OutStatus::THEOREM);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz079_Plus_2) {
    // [       OK ] SolverTest.TPTP_Puz079_Plus_2 (5142 ms)
    solveAndCheck("Problems/PUZ/PUZ079+2.p", Solver::OutStatus::SATISFIABLE);
}

TEST_F(SolverTest, DISABLED_TPTP_Puz080_Plus_2) {
    // [       OK ] SolverTest.TPTP_Puz080_Plus_2 (6635 ms)
    solveAndCheck("Problems/PUZ/PUZ080+2.p", Solver::OutStatus::SATISFIABLE);
}

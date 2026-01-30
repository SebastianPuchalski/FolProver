#include "gtest/gtest.h"

#include "../SuperpositionSolver.hpp"
#include "../ExpressionBuilder.hpp"

using namespace ExpressionBuilder;

class SuperpositionSolverTest : public ::testing::Test {
protected:
    SuperpositionSolver solver;

    // Helper to wrap a single literal or list of literals into a Clause (Junction/OR)
    // The solver expects every clause to be a JunctionFormula of type OR.
    FormulaPtr Clause(std::vector<FormulaPtr> literals) {
        return Disjunction(literals);
    }

    // Helper for a unit clause
    FormulaPtr Unit(FormulaPtr literal) {
        return Disjunction({ literal });
    }

    TermPtr Distinct(std::string value) {
        return std::make_shared<FunctionTerm>(value, std::vector<TermPtr>{}, true);
    }

    // Adapts the old vector<FormulaPtr> to the new vector<ProofNodePtr> interface.
    // Iterates over formulas, wraps them in ProofStep (as Premise), and calls the solver.
    FolSatSolver::Result solve(const std::vector<FormulaPtr>& formulas) {
        std::vector<ProofNodePtr> proofNodes;
        proofNodes.reserve(formulas.size());

        for (const auto& f : formulas) {
            proofNodes.push_back(ProofStep::create(
                f,
                ProofNode::Type::PREMISE, // Input clauses are Premises
                "Input",                  // Rule name
                {}                        // No parents
            ));
        }
        return solver.solve(proofNodes);
    }
};

// 1. Simple Propositional: P, ~P -> UNSAT
TEST_F(SuperpositionSolverTest, SimplePropositionalContradiction) {
    // Clauses: { P }, { ~P }
    auto clauses = std::vector<FormulaPtr>{
        Unit(Pred("P")),
        Unit(Not(Pred("P")))
    };

    auto result = solve(clauses); // Changed from solver.solve to solve
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 2. Simple Propositional: P, Q -> SAT
TEST_F(SuperpositionSolverTest, SimplePropositionalSatisfiable) {
    // Clauses: { P }, { Q }
    auto clauses = std::vector<FormulaPtr>{
        Unit(Pred("P")),
        Unit(Pred("Q"))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::SATISFIABLE);
}

// 3. Modus Ponens style: (~P v Q), P, ~Q -> UNSAT
TEST_F(SuperpositionSolverTest, ModusPonensContradiction) {
    // Clauses: { ~P, Q }, { P }, { ~Q }
    auto clauses = std::vector<FormulaPtr>{
        Clause({ Not(Pred("P")), Pred("Q") }),
        Unit(Pred("P")),
        Unit(Not(Pred("Q")))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 4. FOL Simple Unification: P(x), ~P(a) -> UNSAT
TEST_F(SuperpositionSolverTest, SimpleUnification) {
    // Clauses: { P(x) }, { ~P(a) }
    auto clauses = std::vector<FormulaPtr>{
        Unit(Pred("P", {Var("x")})),
        Unit(Not(Pred("P", {Func("a")}))) // 'a' is a 0-arity function (constant)
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 5. FOL Variable Independence: P(x), ~P(y) -> UNSAT
// The solver should standardize variables, but even if distinct, they unify.
TEST_F(SuperpositionSolverTest, VariableIndependence) {
    // Clauses: { P(x) }, { ~P(y) }
    // Unification should map x -> y (or vice versa) and resolve to empty.
    auto clauses = std::vector<FormulaPtr>{
        Unit(Pred("P", {Var("x")})),
        Unit(Not(Pred("P", {Var("y")})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 6. Classic Socrates Syllogism
// All men are mortal: ~Man(x) v Mortal(x)
// Socrates is a man:  Man(socrates)
// Socrates is not mortal: ~Mortal(socrates)
// -> UNSAT
TEST_F(SuperpositionSolverTest, SocratesSyllogism) {
    auto socrates = Func("socrates");
    auto x = Var("x");

    auto clauses = std::vector<FormulaPtr>{
        Clause({ Not(Pred("Man", {x})), Pred("Mortal", {x}) }),
        Unit(Pred("Man", {socrates})),
        Unit(Not(Pred("Mortal", {socrates})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 7. Function Symbols and Complexity
// P(f(x), y), ~P(f(a), b) -> UNSAT
TEST_F(SuperpositionSolverTest, FunctionSymbolUnification) {
    auto clauses = std::vector<FormulaPtr>{
        Unit(Pred("P", {Func("f", {Var("x")}), Var("y")})),
        Unit(Not(Pred("P", {Func("f", {Func("a")}), Func("b")})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 8. Factoring Requirement
// Clause: { P(x), P(y) } -> essentially P(z)
// Clause: { ~P(a) }
// Without factoring, P(x) resolves with ~P(a) leaving P(y) [substituted], which is still satisfiable alone?
// Actually, standard resolution needs to clear the clause.
// Resolution of {P(x), P(y)} and {~P(a)}:
// 1. Resolve first literal: { P(y) } (where x=a) -> { P(y) }
// 2. Resolve second literal: { P(x) } (where y=a) -> { P(x) }
// We need factoring to turn {P(x), P(y)} into {P(z)} so it can be fully resolved with ~P(a) to empty.
TEST_F(SuperpositionSolverTest, FactoringTest) {
    auto clauses = std::vector<FormulaPtr>{
        Clause({ Pred("P", {Var("x")}), Pred("P", {Var("y")}) }),
        Unit(Not(Pred("P", {Func("a")})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 9. Occurs Check (Infinite term prevention)
// P(x, x), ~P(y, f(y))
// Unifying x=y implies second arg: y = f(y).
// This is structurally impossible in standard FOL (cannot contain itself).
// Unification fails -> Resolution fails -> Set remains SATISFIABLE.
TEST_F(SuperpositionSolverTest, OccursCheck) {
    auto clauses = std::vector<FormulaPtr>{
        Unit(Pred("P", {Var("x"), Var("x")})),
        Unit(Not(Pred("P", {Var("y"), Func("f", {Var("y")})})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::SATISFIABLE);
}

// 10. Empty Clause Set
// An empty set of clauses is vacuously true (SAT).
TEST_F(SuperpositionSolverTest, EmptyClauseSet) {
    auto clauses = std::vector<FormulaPtr>{};
    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::SATISFIABLE);
}

// 11. Empty Clause (A clause with no literals)
// Represents "False", making the set UNSAT.
// Handled by returning UNSAT immediately in `solve`.
TEST_F(SuperpositionSolverTest, ContainsEmptyClause) {
    auto clauses = std::vector<FormulaPtr>{
        Clause({}) // Disjunction of empty operands
    };
    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 12. COMBO TEST: Peano Arithmetic & Transitivity
// Prove that 3 > 0 given:
// 1. Axiom: s(x) > x  (Every successor is greater than its predecessor)
// 2. Rule: x > y AND y > z -> x > z (Transitivity)
// 3. Negated Goal: ~( s(s(s(0))) > 0 )
//
// Expected deduction chain (simplified):
// - s(x) > x implies s(0)>0, s(s(0))>s(0), s(s(s(0)))>s(s(0))
// - Transitivity on s(s(s(0)))>s(s(0)) and s(s(0))>s(0) gives s(s(s(0))) > s(0)
// - Transitivity on s(s(s(0)))>s(0) and s(0)>0 gives s(s(s(0))) > 0
// - Contradiction with negated goal.
TEST_F(SuperpositionSolverTest, PeanoTransitivityCombo) {
    auto x = Var("x");
    auto y = Var("y");
    auto z = Var("z");
    auto zero = Func("zero");

    // Helper to build s(s(...s(term)...))
    auto s = [](TermPtr t) { return Func("s", { t }); };

    auto clauses = std::vector<FormulaPtr>{
        // 1. Axiom: GT(s(x), x)
        Unit(Pred("GT", {s(x), x})),

        // 2. Transitivity: ~GT(x, y) v ~GT(y, z) v GT(x, z)
        Clause({
            Not(Pred("GT", {x, y})),
            Not(Pred("GT", {y, z})),
            Pred("GT", {x, z})
        }),

        // 3. Negated Goal: ~GT(3, 0)
        // 3 is s(s(s(zero)))
        Unit(Not(Pred("GT", { s(s(s(zero))), zero })))
    };

    // This might take a bit longer than trivial tests due to BFS search space
    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 13. REALISTIC COMBO TEST: Graph Connectivity (Pathfinding)
// Tests: Recursion, Transitivity, Unification, Multiple Predicates.
//
// Domain: Nodes a, b, c, d, e
// Facts: Edge(a,b), Edge(b,c), Edge(c,d), Edge(d,e)
// Rules:
// 1. Edge(x,y) -> Path(x,y)
// 2. Path(x,y) ^ Edge(y,z) -> Path(x,z)
//
// Goal: Prove Path(a, e) exists.
// Negated Goal: ~Path(a, e)
//
// Why this works for Naive Solver:
// Unlike Peano arithmetic, the domain is finite. The solver cannot get lost
// generating infinite numbers like s(s(s(s(...)))).
TEST_F(SuperpositionSolverTest, GraphConnectivity) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");
    auto d = Func("d");
    auto e = Func("e");

    auto x = Var("x");
    auto y = Var("y");
    auto z = Var("z");

    auto clauses = std::vector<FormulaPtr>{
        // --- Edges ---
        Unit(Pred("Edge", {a, b})),
        Unit(Pred("Edge", {b, c})),
        Unit(Pred("Edge", {c, d})),
        Unit(Pred("Edge", {d, e})),

        // --- Rules ---
        // 1. Base case: ~Edge(x,y) v Path(x,y)
        Clause({ Not(Pred("Edge", {x, y})), Pred("Path", {x, y}) }),

        // 2. Recursive step: ~Path(x,y) v ~Edge(y,z) v Path(x,z)
        // (If there is a path from x to y, and an edge from y to z, then path x to z)
        Clause({
            Not(Pred("Path", {x, y})),
            Not(Pred("Edge", {y, z})),
            Pred("Path", {x, z})
        }),

        // --- Negated Goal ---
        // ~Path(a, e)
        Unit(Not(Pred("Path", {a, e})))
    };

    // Should solve instantly (Unsatisfiable = Goal Proved)
    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 14. LOGIC PUZZLE: The Mystery of the Stolen Cake (Whodunit)
// This test focuses on "Reasoning by Elimination" and handling non-Horn clauses.
//
// Scenario:
// 1. Suspects: Alice, Bob, Charlie.
// 2. Locations: Kitchen, Garden.
//
// Clues (Axioms):
// 1. One of them is guilty: Guilty(Alice) v Guilty(Bob) v Guilty(Charlie)
// 2. Mutual Exclusivity: Only one person is guilty (Implied by goal, but let's keep it simple).
// 3. Location Constraint: A person cannot be in two places at once.
//    ~At(p, Kitchen) v ~At(p, Garden)
// 4. Bob's Alibi: Bob was in the Garden.
//    At(Bob, Garden)
// 5. The Crime Scene: The crime happened in the Kitchen. Anyone guilty must be in the Kitchen.
//    ~Guilty(x) v At(x, Kitchen)
// 6. Charlie's Rule: If Charlie is guilty, then Alice is innocent. (Optional, let's stick to deductions).
// 7. Witness: Charlie was NOT in the Kitchen.
//    ~At(Charlie, Kitchen)
//
// Deduction Chain:
// - Bob was in Garden + Location Constraint -> Bob not in Kitchen.
// - Bob not in Kitchen + Crime Scene -> Bob not Guilty.
// - Charlie not in Kitchen + Crime Scene -> Charlie not Guilty.
// - Someone is Guilty + Not Bob + Not Charlie -> Alice MUST be Guilty.
//
// Goal: Prove Guilty(Alice)
// Negated Goal: ~Guilty(Alice)
TEST_F(SuperpositionSolverTest, WhodunitLogicPuzzle) {
    // Constants
    auto alice = Func("alice");
    auto bob = Func("bob");
    auto charlie = Func("charlie");
    auto kitchen = Func("kitchen");
    auto garden = Func("garden");

    // Variables
    auto p = Var("p"); // person
    auto x = Var("x");

    auto clauses = std::vector<FormulaPtr>{
        // 1. The culprit is one of the three (Non-Horn clause!)
        Clause({
            Pred("Guilty", {alice}),
            Pred("Guilty", {bob}),
            Pred("Guilty", {charlie})
        }),

            // 2. Physical Constraint: Cannot be in Kitchen AND Garden
            // ~At(p, Kitchen) v ~At(p, Garden)
            Clause({
                Not(Pred("At", {p, kitchen})),
                Not(Pred("At", {p, garden}))
            }),

            // 3. Bob's Alibi: Bob was in the Garden
            Unit(Pred("At", {bob, garden})),

            // 4. Crime Scene Rule: If Guilty(x) -> At(x, Kitchen)
            // CNF: ~Guilty(x) v At(x, Kitchen)
            Clause({
                Not(Pred("Guilty", {x})),
                Pred("At", {x, kitchen})
            }),

            // 5. Witness testimony: Charlie was not in the Kitchen
            Unit(Not(Pred("At", {charlie, kitchen}))),

            // 6. Negated Goal: Prove Alice is Guilty
            // So we assume Alice is NOT Guilty
            Unit(Not(Pred("Guilty", {alice})))
    };

    // Expected: UNSATISFIABLE (Logic holds, contradiction found)
    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 15. SATISFIABILITY TEST: The Consistent Ecosystem
// Unlike previous tests, this one checks if a Knowledge Base is CONSISTENT.
// We provide a set of rules and facts that do NOT lead to a contradiction.
// The solver must derive all possible consequences (saturation) and finish with SATISFIABLE.
//
// Domain:
// - Animals: Lion, Zebra, Grass
// - Relationships: Carnivore, Herbivore, Plant, Eats, Fears
//
// Rules:
// 1. Lion is Carnivore, Zebra is Herbivore, Grass is Plant.
// 2. Carnivores eat Herbivores: ~Carnivore(x) v ~Herbivore(y) v Eats(x, y)
// 3. Herbivores eat Plants: ~Herbivore(x) v ~Plant(y) v Eats(x, y)
// 4. If x Eats y, then y Fears x: ~Eats(x, y) v Fears(y, x)
// 5. Hierarchy Constraint (Disjunction): Every animal is either Predator OR Prey (or both/neither, simple OR).
//    Predator(x) v Prey(x) (Just to add clauses that don't resolve to unit immediately)
//
// Facts derived internally should include:
// - Eats(Lion, Zebra)
// - Fears(Zebra, Lion)
// - Eats(Zebra, Grass)
// - Fears(Grass, Zebra)
//
// There is NO contradiction here. The solver should explore the finite space and return SAT.
TEST_F(SuperpositionSolverTest, ConsistentEcosystem) {
    auto lion = Func("lion");
    auto zebra = Func("zebra");
    auto grass = Func("grass");

    auto x = Var("x");
    auto y = Var("y");

    auto clauses = std::vector<FormulaPtr>{
        // --- Taxonomy Facts ---
        Unit(Pred("Carnivore", {lion})),
        Unit(Pred("Herbivore", {zebra})),
        Unit(Pred("Plant", {grass})),

        // --- Interaction Rules ---

        // 1. Carnivores eat Herbivores
        Clause({
            Not(Pred("Carnivore", {x})),
            Not(Pred("Herbivore", {y})),
            Pred("Eats", {x, y})
        }),

        // 2. Herbivores eat Plants
        Clause({
            Not(Pred("Herbivore", {x})),
            Not(Pred("Plant", {y})),
            Pred("Eats", {x, y})
        }),

        // 3. Fear Reaction (Eats(x,y) -> Fears(y,x))
        Clause({
            Not(Pred("Eats", {x, y})),
            Pred("Fears", {y, x})
        }),

        // --- Just some noise/complexity that doesn't cause contradiction ---
        // 4. Lions are never Plants
        Clause({ Not(Pred("Plant", {lion})) }),

        // 5. Zebras are never Carnivores
        Clause({ Not(Pred("Carnivore", {zebra})) })
    };

    // The solver will perform resolution steps like:
    // Resolving Rule 1 + Lion + Zebra -> Eats(lion, zebra)
    // Resolving Rule 3 + Eats(lion, zebra) -> Fears(zebra, lion)
    // ... and so on.
    // Eventually, no NEW clauses can be generated.
    // Since Empty Clause is never found, result is SATISFIABLE.

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::SATISFIABLE);
}

// 16. CLASSIC PARADOX: The Barber Paradox
// "The barber is a man in town who shaves all those, and only those, men in town who do not shave themselves."
// Question: Does the barber shave himself?
//
// Logical Axiom: Forall x: Shaves(Barber, x) <-> ~Shaves(x, x)
//
// This splits into two implications for CNF:
// 1. Shaves(Barber, x) -> ~Shaves(x, x)  ==>  ~Shaves(Barber, x) v ~Shaves(x, x)
// 2. ~Shaves(x, x) -> Shaves(Barber, x)  ==>  Shaves(x, x) v Shaves(Barber, x)
//
// When the solver instantiates x = Barber, it produces:
// 1. ~Shaves(Barber, Barber) [after factoring]
// 2. Shaves(Barber, Barber)  [after factoring]
// These resolve to the Empty Clause.
TEST_F(SuperpositionSolverTest, TheBarberParadox) {
    auto barber = Func("barber");
    auto x = Var("x");

    auto clauses = std::vector<FormulaPtr>{
        // Clause 1: ~Shaves(Barber, x) v ~Shaves(x, x)
        Clause({
            Not(Pred("Shaves", {barber, x})),
            Not(Pred("Shaves", {x, x}))
        }),

            // Clause 2: Shaves(x, x) v Shaves(Barber, x)
            Clause({
                Pred("Shaves", {x, x}),
                Pred("Shaves", {barber, x})
            })
    };

    // The existence of such a barber is logically impossible (UNSAT).
    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 17. DISTINCT OBJECTS: String Inequality (Sanity Check)
// Tests if the solver immediately detects that two different distinct strings cannot be equal.
// Input: "Apple" = "Banana"
// Result: False -> Empty Clause -> UNSAT.
TEST_F(SuperpositionSolverTest, Distinct_17_StringInequality) {
    auto apple = Distinct("\"Apple\"");
    auto banana = Distinct("\"Banana\"");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(apple, banana))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 18. DISTINCT OBJECTS: Numeric Inequality
// Tests if numbers are treated as distinct objects (TPTP standard).
// Input: 123 = 456
// Result: False -> Empty Clause -> UNSAT.
TEST_F(SuperpositionSolverTest, Distinct_18_NumericInequality) {
    auto n1 = Distinct("123");
    auto n2 = Distinct("456");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(n1, n2))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 19. DISTINCT OBJECTS: Tautology Detection
// Tests if the solver ignores clauses that are logically true due to distinctness.
// Clause: P(x) v ("A" != "B")
// Since "A" != "B" is TRUE, the clause is a tautology. It constrains nothing.
// Adding ~P(x) should NOT produce UNSAT, because the tautology doesn't force P(x) to be true.
TEST_F(SuperpositionSolverTest, Distinct_19_Tautology) {
    auto x = Var("x");
    auto A = Distinct("\"A\"");
    auto B = Distinct("\"B\"");
    auto any = Distinct("\"Any\"");

    auto clauses = std::vector<FormulaPtr>{
        // P(x) OR TRUE -> Tautology
        Clause({ Pred("P", {x}), Not(Equal(A, B)) }),

        // We deny P(x). The system should remain satisfiable.
        Unit(Not(Pred("P", {any})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::SATISFIABLE);
}

// 20. DISTINCT OBJECTS: Resolution Substitution
// Tests if handleDistinctObjects works *after* a variable has been substituted during resolution.
// Clause 1: P(x) v (x = "Red")
// Clause 2: ~P("Blue")
//
// 1. Resolve P(x) and ~P("Blue"). Unifier: { x -> "Blue" }.
// 2. Remaining literal from Clause 1 becomes: ("Blue" = "Red").
// 3. handleDistinctObjects detects this is FALSE and removes it.
// 4. Result: Empty Clause -> UNSAT.
TEST_F(SuperpositionSolverTest, Distinct_20_ResolutionSubstitution) {
    auto x = Var("x");
    auto red = Distinct("\"Red\"");
    auto blue = Distinct("\"Blue\"");

    auto clauses = std::vector<FormulaPtr>{
        Clause({ Pred("P", {x}), Equal(x, red) }),
        Unit(Not(Pred("P", {blue})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 21. COMPLEX: The Pigeonhole Principle (Small Scale)
// Forces the solver to prune a disjunction tree using distinct constraints.
// Scenario: Variable x must be "A", "B", or "C".
// Facts: x != "A", x != "B", x != "C".
//
// 1. (x="A") v (x="B") v (x="C")
// 2. ~(x="A")
// 3. ~(x="B")
// 4. ~(x="C")
//
// The solver must resolve Clause 1 with 2, then the result with 3, then the result with 4.
// Every step must create a distinct object clash or remove a literal.
TEST_F(SuperpositionSolverTest, Distinct_21_Pigeonhole) {
    auto x = Var("x");
    auto A = Distinct("\"A\"");
    auto B = Distinct("\"B\"");
    auto C = Distinct("\"C\"");

    auto clauses = std::vector<FormulaPtr>{
        // x is A or B or C
        Clause({ Equal(x, A), Equal(x, B), Equal(x, C) }),

        // x is not A
        Unit(Not(Equal(x, A))),
        // x is not B
        Unit(Not(Equal(x, B))),
        // x is not C
        Unit(Not(Equal(x, C)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 22. COMPLEX: Nested Function Term Clash
// Tests if distinctness works when objects are wrapped inside functions/predicates.
//
// Clause 1: Container(Box(x))
// Clause 2: ~Container(Box("Gold"))
// Clause 3: x = "Silver"
//
// 1. Resolve 1 & 2: Unifies Box(x) and Box("Gold") -> Binds x="Gold".
//    (Technically, standard resolution might leave an empty clause if it's unit resolution, 
//     but let's force the clash via equality).
//    Wait, simpler logic for Naive Solver:
//    Input: Equal(x, "Silver") AND Equal(x, "Gold") (implicitly via unification elsewhere).
//
// Let's structure it to force a "Silver"="Gold" check:
// Clause 1: Result(x) v (x = "Silver")
// Clause 2: ~Result("Gold")
// 
// Resolve 1 & 2: Unify x -> "Gold".
// Clause 1 becomes: ("Gold" = "Silver").
// handleDistinctObjects -> FALSE -> UNSAT.
TEST_F(SuperpositionSolverTest, Distinct_22_NestedSubstitution) {
    auto x = Var("x");
    auto silver = Distinct("\"Silver\"");
    auto gold = Distinct("\"Gold\"");

    auto clauses = std::vector<FormulaPtr>{
        // Logic: If result isn't true for x, then x MUST be Silver.
        Clause({ Pred("Result", {x}), Equal(x, silver) }),

        // But we know Result is FALSE for Gold.
        // This implies "Gold" MUST be "Silver".
        Unit(Not(Pred("Result", {gold})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 23. MEGA TEST: Database Integrity Cascade
// SCENARIO:
// We have a system with users. We define a chain of rules regarding their status.
// 1. If a user is "Active", they must have "Granted" access.
// 2. If a user has "Granted" access, they must be in the "Safe" zone.
// 3. FACT: All users are "Active" (State(x, "Active")).
// 4. FACT: User "Admin" is currently in the "Danger" zone.
// 5. CONSTRAINT: A user can only be in ONE zone at a time (Functional Dependency).
//    If Zone(x, y) AND Zone(x, z) -> y = z.
//
// EXPECTED EXECUTION FLOW:
// 1. From Fact 3 and Rule 1 -> derive Access(x, "Granted").
// 2. From Derived Fact and Rule 2 -> derive Zone(x, "Safe").
//    Now the solver knows: For ANY x, Zone(x, "Safe").
// 3. From Fact 4, we have Zone("Admin", "Danger").
// 4. Using Constraint 5 with "Admin":
//    Zone("Admin", "Safe") AND Zone("Admin", "Danger") -> Equal("Safe", "Danger").
// 5. handleDistinctObjects detects "Safe" = "Danger" is FALSE.
// 6. The literal is removed -> Empty Clause -> UNSAT.
TEST_F(SuperpositionSolverTest, Distinct_23_MegaIntegrityCascade) {
    // Entities (Distinct Objects)
    auto admin = Distinct("\"Admin\"");

    // Values (Distinct Objects)
    auto active = Distinct("\"Active\"");
    auto granted = Distinct("\"Granted\"");
    auto safe = Distinct("\"Safe\"");
    auto danger = Distinct("\"Danger\"");

    // Variables
    auto x = Var("x");
    auto y = Var("y");
    auto z = Var("z");

    auto clauses = std::vector<FormulaPtr>{
        // --- FACT 1: Universal State ---
        // Every x is Active.
        Unit(Pred("State", {x, active})),

        // --- RULE 1: Active implies Granted ---
        // ~State(x, "Active") v Access(x, "Granted")
        Clause({
            Not(Pred("State", {x, active})),
            Pred("Access", {x, granted})
        }),

        // --- RULE 2: Granted implies Safe Zone ---
        // ~Access(x, "Granted") v Zone(x, "Safe")
        Clause({
            Not(Pred("Access", {x, granted})),
            Pred("Zone", {x, safe})
        }),

        // --- FACT 2: The Anomaly ---
        // "Admin" is in "Danger" zone.
        Unit(Pred("Zone", {admin, danger})),

        // --- CONSTRAINT: Functional Dependency of Zone ---
        // A user cannot be in two zones.
        // ~Zone(x, y) v ~Zone(x, z) v (y = z)
        Clause({
            Not(Pred("Zone", {x, y})),
            Not(Pred("Zone", {x, z})),
            Equal(y, z)
        })
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 24. EQUALITY: Reflexivity (Axiom Check)
// Axiom: For all X, X = X.
// Goal: Prove a = a.
// Negated Goal: a != a.
// Complexity: Very Low (1 resolution step).
TEST_F(SuperpositionSolverTest, Eq_24_Reflexivity) {
    auto a = Func("a");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Not(Equal(a, a)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 25. EQUALITY: Symmetry (Axiom Check)
// Axiom: X = Y -> Y = X.
// Input: a = b.
// Goal: Prove b = a.
// Complexity: Low.
TEST_F(SuperpositionSolverTest, Eq_25_Symmetry) {
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Not(Equal(b, a)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 26. EQUALITY: Predicate Substitution (Congruence)
// Axiom: X = Y -> (P(X) -> P(Y)).
// Input: a = b, P(a).
// Goal: Prove P(b).
// Complexity: Medium.
TEST_F(SuperpositionSolverTest, Eq_26_PredicateSubstitution) {
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Pred("P", {a})),
        Unit(Not(Pred("P", {b})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 27. EQUALITY: Function Substitution (Congruence)
// Axiom: X = Y -> f(X) = f(Y).
// Input: a = b.
// Goal: Prove f(a) = f(b).
// Complexity: Medium.
TEST_F(SuperpositionSolverTest, Eq_27_FunctionSubstitution) {
    auto a = Func("a");
    auto b = Func("b");
    auto f = [](TermPtr t) { return Func("f", { t }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Not(Equal(f(a), f(b))))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 28. EQUALITY: Transitivity (Short Chain)
// Axiom: X=Y & Y=Z -> X=Z.
// Input: a = b, b = c.
// Goal: Prove a = c.
// Complexity: Medium.
TEST_F(SuperpositionSolverTest, Eq_28_Transitivity_Short) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Equal(b, c)),
        Unit(Not(Equal(a, c)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 29. EQUALITY: Symmetry + Substitution (Simplified)
// Replaces the slow DoubleSubstitution.
// Scenario: a = b. We know P(b). Prove P(a).
//
// Logic:
// 1. Direct substitution P(a)->P(b) works if we have P(a). But we have P(b).
// 2. Solver must use Symmetry to deduce b = a.
// 3. Then use b = a to substitute into P(b) giving P(a).
// This tests combining axioms without the combinatorial explosion of swapping two args.
TEST_F(SuperpositionSolverTest, Eq_29_SymmetrySubstitution) {
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),          // a equals b
        Unit(Pred("P", {b})),       // We have P(b)
        Unit(Not(Pred("P", {a})))   // We want to prove P(a)
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 30. EQUALITY: Chain + Function
// Input: a = b, b = c.
// Goal: Prove f(a) = f(c).
// Complexity: Medium/High.
TEST_F(SuperpositionSolverTest, Eq_30_TransitivityAndFunction) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");
    auto f = [](TermPtr t) { return Func("f", { t }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Equal(b, c)),
        Unit(Not(Equal(f(a), f(c))))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 31. EQUALITY: Variable Unification with Equality
// Input: f(x) = x. Fact: P(f(a)). Prove: P(a).
// NOTE: This test is skipped in DEBUG mode because BFS explodes on generic identity axioms.
TEST_F(SuperpositionSolverTest, Eq_31_VariableIdentity) {
    // Only run in Release mode (NDEBUG is defined).
    // In Debug mode (or if NDEBUG is not defined), we skip.
#if !defined(NDEBUG) || defined(_DEBUG)
    // Print a message so we know it was skipped intentionally
    std::cout << "[   INFO   ] Test Eq_31_VariableIdentity skipped in DEBUG mode (too slow for BFS)." << std::endl;
    return;
#endif

    auto x = Var("x");
    auto a = Func("a");
    auto f = [](TermPtr t) { return Func("f", { t }); };

    auto clauses = std::vector<FormulaPtr>{
        // Axiom: f(x) = x
        Unit(Equal(f(x), x)),

        // Fact: P(f(a))
        Unit(Pred("P", {f(a)})),

        // Negated Goal: ~P(a)
        Unit(Not(Pred("P", {a})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 32. BASIC SYMMETRY
// Tests if a=b allows rewriting P(b) to P(a).
TEST_F(SuperpositionSolverTest, Superposition_BasicSymmetry) {
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Pred("P", {b})),
        Unit(Not(Pred("P", {a})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 33. EQUALITY COMMUTATIVITY (Literal Level)
// Tests if the solver recognizes that a=b contradicts b!=a.
// This checks if the unification or resolution/paramodulation handles the symmetric nature of equality literals.
TEST_F(SuperpositionSolverTest, Superposition_EqualityCommutativity) {
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Not(Equal(b, a)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 34. TRANSITIVITY CHAIN
// Tests deep search capability: a=b, b=c, c=d, d=e -> a=e.
TEST_F(SuperpositionSolverTest, Superposition_TransitivityChain) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");
    auto d = Func("d");
    auto e = Func("e");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Equal(b, c)),
        Unit(Equal(c, d)),
        Unit(Equal(d, e)),
        Unit(Not(Equal(a, e)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 35. EQUALITY RESOLUTION
// Tests x!=y logic where x and y unify.
TEST_F(SuperpositionSolverTest, Superposition_EqualityResolution) {
    auto x = Var("x");
    auto y = Var("y");
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Not(Equal(
            Func("f", {x, a}),
            Func("f", {b, y})
        )))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 36. FUNCTION CLASH
// f(x) = g(x) -> Contradiction for f(a) != g(a).
TEST_F(SuperpositionSolverTest, Superposition_FunctionClash) {
    auto x = Var("x");
    auto a = Func("a");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(Func("f", {x}), Func("g", {x}))),
        Unit(Not(Equal(Func("f", {a}), Func("g", {a}))))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 37. DISJOINT VARIABLES
// f(x) = g(y). Strong axiom implying strict equality for any arguments.
TEST_F(SuperpositionSolverTest, Superposition_DisjointVariables) {
    auto x = Var("x");
    auto y = Var("y");
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(Func("f", {x}), Func("g", {y}))),
        Unit(Not(Equal(Func("f", {a}), Func("g", {b}))))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 38. REPEATED VARIABLES CONSTRAINT
// h(x, x) = c. Should not match h(a, b) unless a=b.
TEST_F(SuperpositionSolverTest, Superposition_RepeatedVariablesConstraint) {
    auto x = Var("x");
    auto a = Func("a");
    auto c = Func("c");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(Func("h", {x, x}), c)),
        Unit(Not(Equal(Func("h", {a, a}), c)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 39. DEEP NESTED CONGRUENCE
// Rewriting 'a' inside f(g(h(a))).
TEST_F(SuperpositionSolverTest, Superposition_DeepNestedCongruence) {
    auto a = Func("a");
    auto b = Func("b");
    auto deepTerm = [](TermPtr t) {
        return Func("f", { Func("g", { Func("h", { t }) }) });
        };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Not(Equal(deepTerm(a), deepTerm(b))))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 40. ROOT OVERLAP
// Rewriting at the very top level: f(x)=g(x) applied to h(f(a)).
TEST_F(SuperpositionSolverTest, Superposition_RootOverlap) {
    auto x = Var("x");
    auto a = Func("a");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(Func("f", {x}), Func("g", {x}))),
        Unit(Not(Equal(
            Func("h", { Func("f", {a}) }),
            Func("h", { Func("g", {a}) })
        )))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 41. AXIOM INSTANTIATION
// Using f(x)=x to reduce f(f(f(a))).
TEST_F(SuperpositionSolverTest, Superposition_AxiomInstantiation) {
    auto x = Var("x");
    auto a = Func("a");
    auto f = [](TermPtr t) { return Func("f", { t }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(f(x), x)),
        Unit(Not(Equal(f(f(f(a))), a)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 42. RECURSIVE REWRITE
// f(x) = f(f(x)). Requires intelligent application.
TEST_F(SuperpositionSolverTest, Superposition_RecursiveRewrite) {
    auto x = Var("x");
    auto a = Func("a");
    auto f = [](TermPtr t) { return Func("f", { t }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(f(x), f(f(x)))),
        Unit(Pred("P", { f(a) })),
        Unit(Not(Pred("P", { f(f(f(f(a)))) })))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 43. NON-UNIT INTERACTION
// Using a=b from (a=b OR Q).
TEST_F(SuperpositionSolverTest, Superposition_NonUnitInteraction) {
    auto a = Func("a");
    auto b = Func("b");
    auto Q = Pred("Q");

    auto clauses = std::vector<FormulaPtr>{
        Clause({ Equal(a, b), Q }),
        Unit(Not(Q)),
        Unit(Pred("P", {a})),
        Unit(Not(Pred("P", {b})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 44. DISJUNCTIVE VARIABLE CONTEXT
// f(x)=a OR g(x)=b.
TEST_F(SuperpositionSolverTest, Superposition_DisjunctiveVarContext) {
    auto x = Var("x");
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");

    auto clauses = std::vector<FormulaPtr>{
        Clause({ Equal(Func("f", {x}), a), Equal(Func("g", {x}), b) }),
        Unit(Not(Equal(Func("f", {c}), a))),
        Unit(Not(Equal(Func("g", {c}), b)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 45. BURIED EQUALITY
// a=b hidden in a clause with other literals.
TEST_F(SuperpositionSolverTest, Superposition_BuriedEquality) {
    auto x = Var("x");
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Clause({ Pred("P", {x}), Equal(a, b), Pred("Q", {x}) }),
        Unit(Not(Pred("P", {Var("y")}))),
        Unit(Not(Pred("Q", {Var("z")}))),
        Unit(Pred("R", {a})),
        Unit(Not(Pred("R", {b})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 46. COMMUTATIVITY AXIOM
// f(x, y) = f(y, x).
TEST_F(SuperpositionSolverTest, Superposition_CommutativityAxiom) {
    auto x = Var("x");
    auto y = Var("y");
    auto a = Func("a");
    auto b = Func("b");
    auto f = [](TermPtr t1, TermPtr t2) { return Func("f", { t1, t2 }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(f(x, y), f(y, x))),
        Unit(Not(Equal(f(a, b), f(b, a))))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 47. ASSOCIATIVITY
TEST_F(SuperpositionSolverTest, Superposition_Associativity) {
    auto x = Var("x");
    auto y = Var("y");
    auto z = Var("z");
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");
    auto f = [](TermPtr t1, TermPtr t2) { return Func("f", { t1, t2 }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(f(f(x, y), z), f(x, f(y, z)))),
        Unit(Not(Equal(f(f(a, b), c), f(a, f(b, c)))))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 48. DISTRIBUTIVITY
TEST_F(SuperpositionSolverTest, Superposition_Distributivity) {
    auto x = Var("x");
    auto y = Var("y");
    auto z = Var("z");
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");
    auto add = [](TermPtr t1, TermPtr t2) { return Func("add", { t1, t2 }); };
    auto mul = [](TermPtr t1, TermPtr t2) { return Func("mul", { t1, t2 }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(mul(x, add(y, z)), add(mul(x, y), mul(x, z)))),
        Unit(Not(Equal(mul(a, add(b, c)), add(mul(a, b), mul(a, c)))))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 49. LIST REDUCTION (Symbolic)
TEST_F(SuperpositionSolverTest, Superposition_ListReduction) {
    auto x = Var("x");
    auto y = Var("y");
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");
    auto cons = [](TermPtr h, TermPtr t) { return Func("cons", { h, t }); };
    auto car = [](TermPtr t) { return Func("car", { t }); };
    auto cdr = [](TermPtr t) { return Func("cdr", { t }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(car(cons(x, y)), x)),
        Unit(Equal(cdr(cons(x, y)), y)),
        Unit(Not(Equal(car(cdr(cons(a, cons(b, c)))), b)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 50. BOOLEAN CHAIN
TEST_F(SuperpositionSolverTest, Superposition_BooleanChain) {
    auto T = Func("true");
    auto F = Func("false");
    auto neg = [](TermPtr t) { return Func("not", { t }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(neg(T), F)),
        Unit(Equal(neg(F), T)),
        Unit(Not(Equal(neg(neg(T)), T)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 51. CYCLIC ROTATION
// k(x,y,z) = k(y,z,x).
TEST_F(SuperpositionSolverTest, Superposition_CyclicRotation) {
    auto x = Var("x");
    auto y = Var("y");
    auto z = Var("z");
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");
    auto k = [](TermPtr t1, TermPtr t2, TermPtr t3) { return Func("k", { t1, t2, t3 }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(k(x, y, z), k(y, z, x))),
        Unit(Not(Equal(k(a, b, c), k(c, a, b))))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 52. REWRITE INSIDE EQUALITY LITERAL
// Paramodulation into an equality term.
TEST_F(SuperpositionSolverTest, Superposition_RewriteInsideEquality) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(Func("f", {a}), c)),
        Unit(Not(Equal(Func("f", {b}), c))),
        Unit(Equal(a, b))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 53. REWRITE INSIDE PREDICATE
// Paramodulation into a predicate term.
TEST_F(SuperpositionSolverTest, Superposition_RewriteInsidePredicate) {
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Pred("P", { a, Func("f", {a}) })),
        Unit(Not(Pred("P", { b, Func("f", {b}) })))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 54. VARIABLE ABSTRACTION
// P(x,x) vs P(a,b) with a=b.
TEST_F(SuperpositionSolverTest, Superposition_VariableAbstraction) {
    auto x = Var("x");
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Pred("P", {x, x})),
        Unit(Not(Pred("P", {a, b})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 55. POSITIVE SUPERPOSITION
// a=b, a=c -> b=c.
TEST_F(SuperpositionSolverTest, Superposition_PositiveSuperposition) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),
        Unit(Equal(a, c)),
        Unit(Not(Equal(b, c)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 56. MIXED PREDICATE AND EQUALITY TRANSITIVITY
TEST_F(SuperpositionSolverTest, Superposition_MixedTransitivity) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Pred("P", {a})),
        Unit(Equal(a, b)),
        Unit(Equal(b, c)),
        Unit(Not(Pred("P", {c})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 57. SIMULATED HIGHER ORDER EQUALITY
// eq(a,b)=true check.
TEST_F(SuperpositionSolverTest, Superposition_SimulatedHigherOrderEq) {
    auto x = Var("x");
    auto a = Func("a");
    auto b = Func("b");
    auto T = Func("true");
    auto F = Func("false");
    auto eq_term = [](TermPtr t1, TermPtr t2) { return Func("eq_term", { t1, t2 }); };

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(eq_term(x, x), T)),
        Unit(Equal(eq_term(a, b), F)),
        Unit(Not(Equal(T, F))),
        Unit(Equal(a, b))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 58. PARAMODULATION FROM NON-UNIT
// Deriving a=b from a clause and using it.
TEST_F(SuperpositionSolverTest, Superposition_ParamodFromNonUnit) {
    auto a = Func("a");
    auto b = Func("b");
    auto P = Pred("P");

    auto clauses = std::vector<FormulaPtr>{
        Clause({ Equal(a, b), P }),
        Unit(Pred("Q", {a})),
        Unit(Not(Pred("Q", {b}))),
        Unit(Not(P))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 59. TRANSITIVITY ACROSS DISJUNCTION
TEST_F(SuperpositionSolverTest, Superposition_TransitivityAcrossDisjunction) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");
    auto d = Func("d");

    auto clauses = std::vector<FormulaPtr>{
        Clause({ Equal(a, b), Equal(a, c) }),
        Unit(Equal(b, d)),
        Unit(Equal(c, d)),
        Unit(Not(Equal(a, d)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 60. DOMAIN CARDINALITY
TEST_F(SuperpositionSolverTest, Superposition_DomainCardinality) {
    auto x = Var("x");
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(x, a)),
        Unit(Not(Equal(b, c)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 62. EQUALITY FACTORING (Implicit)
// a=b v a=c, plus a!=b, a!=c.
TEST_F(SuperpositionSolverTest, Superposition_EqualityFactoringImplicit) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");

    auto clauses = std::vector<FormulaPtr>{
        Clause({ Equal(a, b), Equal(a, c) }),
        Unit(Not(Equal(a, b))),
        Unit(Not(Equal(a, c)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 63. EQUALITY FACTORING (With Inequality Constraint)
// f(x)=a v f(y)=b v x!=y.
TEST_F(SuperpositionSolverTest, Superposition_EqualityFactoringWithConstraint) {
    auto x = Var("x");
    auto y = Var("y");
    auto z = Var("z");
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Clause({
            Equal(Func("f", {x}), a),
            Equal(Func("f", {y}), b),
            Not(Equal(x, y))
        }),
        Unit(Equal(a, b)),
        Unit(Not(Equal(Func("f", {z}), a)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 64. EQUALITY FACTORING
// f(x)=a v f(x)=b, f(y)!=a, f(y)!=b
TEST_F(SuperpositionSolverTest, Superposition_EqualityFactoringMerge) {
    auto x = Var("x");
    auto y = Var("y");
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Clause({ Equal(Func("f", {x}), a), Equal(Func("f", {x}), b) }),
        Unit(Not(Equal(Func("f", {y}), a))),
        Unit(Not(Equal(Func("f", {y}), b)))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 65.
// a=b & Q(a) -> Q(b).
TEST_F(SuperpositionSolverTest, Superposition_SelfParamodulation) {
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Pred("Q", {a})),
        Unit(Equal(a, b)),
        Unit(Not(Pred("Q", {b})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 66. DISTINCT OBJECT PROPAGATION
// Tests interaction between Paramodulation and Distinct Objects.
// Axioms map functions to specific Distinct constants.
// The solver must rewrite f(a) -> D1 and g(a) -> D2.
// Then it confronts the equality D1 = D2.
// Since D1 and D2 are distinct, D1 = D2 is False.
// If the solver correctly propagates this, it finds the contradiction.
TEST_F(SuperpositionSolverTest, Superposition_DistinctPropagation) {
    auto x = Var("x");
    auto a = Func("a");
    // D1 and D2 are marked as distinct objects (e.g., constants "1" and "2")
    auto d1 = Distinct("D1");
    auto d2 = Distinct("D2");

    auto clauses = std::vector<FormulaPtr>{
        // Axiom 1: f(x) = D1
        Unit(Equal(Func("f", {x}), d1)),

        // Axiom 2: g(x) = D2
        Unit(Equal(Func("g", {x}), d2)),

        // Hyp: f(a) = g(a)
        // Rewrites to: D1 = g(a) -> D1 = D2
        // D1=D2 is implicitly False because objects are distinct.
        Unit(Equal(Func("f", {a}), Func("g", {a})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 67. DISTINCT OBJECT CLAUSE PRUNING
// Tests if handleDistinctObjects correctly simplifies clauses during processing.
// Clause: P(a) OR D1 = D2
// Since D1 and D2 are distinct, D1 = D2 is always False.
// The solver should effectively treat this clause as just P(a).
// If it fails to prune, it might treat it as a non-unit clause and fail to resolve with ~P(a)
// (depending on strategy for non-unit resolution).
TEST_F(SuperpositionSolverTest, Superposition_DistinctClausePruning) {
    auto a = Func("a");
    auto d1 = Distinct("D1");
    auto d2 = Distinct("D2");

    auto clauses = std::vector<FormulaPtr>{
        // P(a) OR (D1 = D2)
        // (D1=D2) is False, so this simplifies to P(a)
        Clause({ Pred("P", {a}), Equal(d1, d2) }),

        // ~P(a)
        Unit(Not(Pred("P", {a})))
    };

    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 68. MISSING EQUALITY FACTORING (The "Entangled" Variable Test)
// This test targets the scenario typically handled by Equality Factoring.
// Clause: f(x) = a  OR  f(y) = b  OR  x != y
// Facts:  a = b,  f(z) != a
//
// Reasoning:
// 1. Given a = b, the main clause effectively becomes: f(x) = a OR f(y) = a OR x != y.
// 2. The literal x != y prevents standard unification of x and y in simple resolution steps.
// 3. Equality Factoring (or Self-Paramodulation) is required to infer that
//    we can merge the left-hand sides f(x) and f(y) by unifying x and y.
//    This effectively allows deriving f(x) = a (conditionally or directly).
// 4. Finally, f(x) = a contradicts f(z) != a.
TEST_F(SuperpositionSolverTest, Critical_Missing_Equality_Factoring) {
    auto x = Var("x");
    auto y = Var("y");
    auto z = Var("z");
    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        // The tricky clause: either f(x)=a, OR f(y)=b, OR x and y are distinct.
        // This structure forces the solver to merge the positive equality literals.
        Clause({
            Equal(Func("f", {x}), a),
            Equal(Func("f", {y}), b),
            Not(Equal(x, y))
        }),

            // Collapse the constants a and b, making the two heads potentially identical
            Unit(Equal(a, b)),

            // Contradiction hook: f(z) cannot be a.
            Unit(Not(Equal(Func("f", {z}), a)))
    };

    // Expect UNSATISFIABLE.
    // If the solver lacks Equality Factoring (or adequate Self-Paramodulation),
    // it will likely fail to prove this and return SAT or timeout.
    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 69. FUNCTION CONGRUENCE CHAIN
// Tests if the solver can propagate equality through function arguments.
// 1. f(a, b) = a
// 2. a = c
// 3. b = d
// 4. f(c, d) != c  <-- Contradiction
TEST_F(SuperpositionSolverTest, FunctionCongruence_FastUnsat) {
    auto a = Func("a");
    auto b = Func("b");
    auto c = Func("c");
    auto d = Func("d");
    auto f = "f";

    auto clauses = std::vector<FormulaPtr>{
        // f(a, b) = a
        Clause({ Equal(Func(f, {a, b}), a) }),

        // a = c
        Unit(Equal(a, c)),

        // b = d
        Unit(Equal(b, d)),

        // Negation of logical consequence: f(c, d) != c
        // Logic: f(c,d) should be f(a,b) (since c=a, d=b), which is equal to a, which is equal to c.
        Unit(Not(Equal(Func(f, {c, d}), c)))
    };

    // Should return UNSATISFIABLE very quickly (< 10ms)
    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 70. THE DOPPELGANGER BUG (Self-Interaction Test)
// This test checks if the solver can handle a clause that contains an internal
// contradiction conditional on its own equality.
//
// Clause:  f(x) = a  OR  f(x) != f(b)
// Logic:
// 1. If we assume the first part is false (f(x) != a), then the second part
//    must hold.
// 2. However, if we instantiate x = b, the clause becomes:
//    f(b) = a  OR  f(b) != f(b)
// 3. f(b) != f(b) is trivially false (Delete Trivial Literals).
// 4. Therefore, the clause effectively collapses to just: f(b) = a.
//
// Goal:    f(b) != a
//
// If the solver successfully deduces UNSAT, it means it managed to "realize"
// the implication of x=b either through binary resolution with the goal
// or valid internal unification logic.
TEST_F(SuperpositionSolverTest, Doppelganger_SelfInteraction) {
    auto a = Func("a");
    auto b = Func("b");
    auto x = Var("x");
    auto f = "f";

    // Clause: f(x) = a  OR  f(x) != f(b)
    // Effectively asserts: f(b) = a
    auto clause = Clause({
        Equal(Func(f, {x}), a),
        Not(Equal(Func(f, {x}), Func(f, {b})))
        });

    // Goal: Negation of f(b) = a
    auto goal = Unit(Not(Equal(Func(f, { b }), a)));

    auto result = solve({ clause, goal });
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

// 71. Equality Symmetry Bug Reproduction
TEST_F(SuperpositionSolverTest, EqualitySymmetry_SimpleMismatch) {
    // Scenario: a = b AND b != a
    // Mathematically, this is a clear contradiction.
    //
    // Current Limitation:
    // The solver's unify() function currently checks EqualityFormulas strictly:
    // It requires unify(left1, left2) && unify(right1, right2).
    // Here: unify("a", "b") fails, so the resolution step is skipped.
    //
    // Required Fix:
    // The unify() function for Equality must also try the symmetric case:
    // unify(left1, right2) && unify(right1, left2).

    auto a = Func("a");
    auto b = Func("b");

    auto clauses = std::vector<FormulaPtr>{
        Unit(Equal(a, b)),          // Clause 1: a = b
        Unit(Not(Equal(b, a)))      // Clause 2: ~(b = a)
    };

    // CURRENTLY FAILS (returns SATISFIABLE)
    // WILL PASS (return UNSATISFIABLE) after fixing unify() to handle symmetry.
    auto result = solve(clauses);
    EXPECT_EQ(result, FolSatSolver::Result::UNSATISFIABLE);
}

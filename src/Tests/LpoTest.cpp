#include "gtest/gtest.h"
#include "../Lpo.hpp"
#include "../ExpressionBuilder.hpp"

using namespace ExpressionBuilder;

class LpoTest : public ::testing::Test {
protected:
    Lpo lpo;

    // --- Common Variables ---
    VariableTermPtr x = Var("x");
    VariableTermPtr y = Var("y");
    VariableTermPtr z = Var("z");

    // --- Common Constants (Functions with arity 0) ---
    // Lexicographical order: "a" < "b" < "c"
    FunctionTermPtr a = Func("a");
    FunctionTermPtr b = Func("b");
    FunctionTermPtr c = Func("c");

    // --- Distinct Objects (Numbers/Strings) ---
    TermPtr d100 = Distinct("100");
    TermPtr d200 = Distinct("200");

    // --- Helper Factories ---
    // f, g, h for standard functions
    FunctionTermPtr f(std::vector<TermPtr> args) { return Func("f", args); }
    FunctionTermPtr g(std::vector<TermPtr> args) { return Func("g", args); }
    FunctionTermPtr h(std::vector<TermPtr> args) { return Func("h", args); }

    // P, Q for predicates
    PredicateFormulaPtr P(std::vector<TermPtr> args) { return Pred("P", args); }
    PredicateFormulaPtr Q(std::vector<TermPtr> args) { return Pred("Q", args); }
};

// ============================================================================
// GROUP 1: BASIC PROPERTIES (Reflexivity, Irreflexivity, Variables)
// ============================================================================

TEST_F(LpoTest, Property_Reflexivity) {
    // s = s
    EXPECT_TRUE(lpo.isEqual(x, x));
    EXPECT_TRUE(lpo.isEqual(f({ a }), f({ a })));
}

TEST_F(LpoTest, Property_Irreflexivity) {
    // s !> s
    EXPECT_FALSE(lpo.isGreater(x, x));
    EXPECT_FALSE(lpo.isGreater(f({ a }), f({ a })));
}

TEST_F(LpoTest, Variables_DifferentAreIncomparable) {
    // x vs y -> Not Comparable (NC) because substitution is unknown
    EXPECT_TRUE(lpo.isIncomparable(x, y));
    EXPECT_TRUE(lpo.isIncomparable(y, x));
}

TEST_F(LpoTest, Variables_OccursCheck) {
    // x in f(x) -> f(x) > x
    EXPECT_TRUE(lpo.isGreater(f({ x }), x));

    // Deep occurrence: f(g(h(x))) > x
    EXPECT_TRUE(lpo.isGreater(f({ g({h({x})}) }), x));
}

// ============================================================================
// GROUP 2: PRECEDENCE HIERARCHY (Type > Arity > Name)
// ============================================================================

TEST_F(LpoTest, Hierarchy_Type_PredicateDominatesFunction) {
    // P(a) > f(a)
    // Even if names were inverted, Predicate type weight (4) > Function weight (2)
    EXPECT_TRUE(lpo.isGreater(P({ a }), f({ a })));
}

TEST_F(LpoTest, Hierarchy_Type_FunctionDominatesDistinct) {
    // f > "distinct_obj"
    EXPECT_TRUE(lpo.isGreater(f({}), d100));
}

TEST_F(LpoTest, Hierarchy_Arity_HighArityDominatesLowArity) {
    // f(x, y) [arity 2] vs g(x) [arity 1]
    // Even if "g" > "f" lexicographically, Arity 2 takes precedence over Name.
    EXPECT_TRUE(lpo.isGreater(f({ x, y }), g({ x })));

    // Same symbol, different arity
    // f(a, b) > f(a)
    EXPECT_TRUE(lpo.isGreater(f({ a, b }), f({ a })));
}

TEST_F(LpoTest, Hierarchy_Name_LexicographicalOrder) {
    // Same Type (Function), Same Arity (1).
    // "g" > "f" implies g(x) > f(x)
    EXPECT_TRUE(lpo.isGreater(g({ x }), f({ x })));

    // Constants: "b" > "a"
    EXPECT_TRUE(lpo.isGreater(b, a));

    // Distinct objects: "200" > "100"
    EXPECT_TRUE(lpo.isGreater(d200, d100));
}

// ============================================================================
// GROUP 3: SUBTERM PROPERTY
// Term is greater than any of its proper subterms.
// ============================================================================

TEST_F(LpoTest, Subterm_DirectArguments) {
    // f(a, b) > a
    EXPECT_TRUE(lpo.isGreater(f({ a, b }), a));

    // f(a, b) > b
    EXPECT_TRUE(lpo.isGreater(f({ a, b }), b));
}

TEST_F(LpoTest, Subterm_NestedArguments) {
    // h(g(a)) > a
    EXPECT_TRUE(lpo.isGreater(h({ g({a}) }), a));
}

// ============================================================================
// GROUP 4: LEXICOGRAPHIC DECOMPOSITION (Functions)
// Applied when Heads are identical (f = g).
// ============================================================================

TEST_F(LpoTest, Lexicographic_LeftmostDifference) {
    // f(b, a) vs f(a, b)
    // Index 0: b > a.
    // Stability check: f(b, a) > a (arg 0 of RHS)? Yes (subterm).
    // Stability check: f(b, a) > b (arg 1 of RHS)? Yes (subterm b in LHS).
    EXPECT_TRUE(lpo.isGreater(f({ b, a }), f({ a, b })));
}

TEST_F(LpoTest, Lexicographic_LaterDifference) {
    // f(a, b) vs f(a, a)
    // Index 0: a == a.
    // Index 1: b > a.
    // Stability: f(a, b) > a (arg 1 of RHS)? Yes (subterm).
    EXPECT_TRUE(lpo.isGreater(f({ a, b }), f({ a, a })));
}

TEST_F(LpoTest, Lexicographic_StabilityFailure_ReturnsNC) {
    // f(x, b) vs f(a, b)
    // Index 0: x vs a -> Incomparable.
    // Comparison must stop immediately.
    EXPECT_TRUE(lpo.isIncomparable(f({ x, b }), f({ a, b })));
}

// ============================================================================
// GROUP 5: EQUALITY & MULTISET EXTENSION (Rule 2.4)
// Verifies that s=t is treated as the multiset {s, t}.
// Order is determined by:
// 1. Removing common elements.
// 2. Checking if every remaining element in RHS is dominated by some element in LHS.
// ============================================================================

TEST_F(LpoTest, Equality_Symmetry_A_Equals_B_Is_B_Equals_A) {
    // (a = b) <=> {a, b}
    // (b = a) <=> {b, a}
    // Sets are identical.
    auto eq1 = Equal(a, b);
    auto eq2 = Equal(b, a);

    EXPECT_TRUE(lpo.isEqual(eq1, eq2));
}

TEST_F(LpoTest, Equality_Dominance_Simple) {
    // (b = b) vs (a = a) where b > a.
    // {b, b} vs {a, a}.
    // b > a. Result: GREATER.
    auto eqBig = Equal(b, b);
    auto eqSmall = Equal(a, a);

    EXPECT_TRUE(lpo.isGreater(eqBig, eqSmall));
}

TEST_F(LpoTest, Equality_Dominance_WithCommonTerm) {
    // (f(x) = b) vs (f(x) = a).
    // Multisets: {f(x), b} vs {f(x), a}.
    // Step 1: Remove common term f(x).
    // Step 2: Compare {b} vs {a}.
    // Since b > a, LHS > RHS.
    auto common = f({ x });
    auto eqLhs = Equal(common, b);
    auto eqRhs = Equal(common, a);

    EXPECT_TRUE(lpo.isGreater(eqLhs, eqRhs));
}

TEST_F(LpoTest, Equality_Complex_MultisetLogic) {
    // LHS: (c = a) -> {c, a}
    // RHS: (b = b) -> {b, b}
    // Precedence: c > b > a.
    // Logic:
    // 1. No common elements.
    // 2. Check RHS elements:
    //    - Is first 'b' dominated by something in LHS? Yes, c > b.
    //    - Is second 'b' dominated by something in LHS? Yes, c > b.
    // Result: GREATER.
    auto eqLhs = Equal(c, a);
    auto eqRhs = Equal(b, b);

    EXPECT_TRUE(lpo.isGreater(eqLhs, eqRhs));
}

TEST_F(LpoTest, Equality_FailureCase) {
    // LHS: (b = a) -> {b, a}
    // RHS: (c = a) -> {c, a}
    // Common: {a}. Remaining: LHS={b}, RHS={c}.
    // b < c.
    // Result: LESS.
    auto eqLhs = Equal(b, a);
    auto eqRhs = Equal(c, a);

    EXPECT_TRUE(lpo.isLess(eqLhs, eqRhs));
}

// ============================================================================
// GROUP 6: NEGATION HANDLING
// Rules:
// 1. Negation is transparent for comparison (compare atoms).
// 2. If atoms equal: Negated > Non-Negated.
// ============================================================================

TEST_F(LpoTest, Negation_Dominates_Positve_If_Atoms_Equal) {
    // ~P(a) > P(a)
    auto atom = P({ a });
    auto neg = Not(atom);

    EXPECT_TRUE(lpo.isGreater(neg, atom));
}

TEST_F(LpoTest, Negation_Transparent_If_Atoms_Different) {
    // ~P(b) > ~P(a) because b > a
    auto negBig = Not(P({ b }));
    auto negSmall = Not(P({ a }));

    EXPECT_TRUE(lpo.isGreater(negBig, negSmall));
}

TEST_F(LpoTest, Negation_Mixed_Dominance) {
    // ~P(b) > P(a)
    // Compare atoms: P(b) > P(a).
    // Since atoms are not equal, the negation sign weight doesn't override atom order.
    auto negBig = Not(P({ b }));
    auto posSmall = P({ a });

    EXPECT_TRUE(lpo.isGreater(negBig, posSmall));
}

TEST_F(LpoTest, Negation_Mixed_Subordinate) {
    // ~P(a) vs P(b)
    // Atoms: P(a) < P(b).
    // Even though LHS is negative (heavy), the atom content is lighter.
    // Result: LESS.
    auto negSmall = Not(P({ a }));
    auto posBig = P({ b });

    EXPECT_TRUE(lpo.isLess(negSmall, posBig));
}

// ============================================================================
// GROUP 7: COMPLEX RECURSION ("GRANDFATHER" SCENARIOS)
// Testing deep interaction between Precedence and Argument Dominance.
// ============================================================================

TEST_F(LpoTest, Recursion_PrecedenceDominatesStructure) {
    // h > f (lexicographically).
    // Compare: h(f(x)) vs f(h(x))
    // 1. Head Check: h > f.
    // 2. Stability Check (Alpha):
    //    Does LHS (h(f(x))) dominate all args of RHS (h(x))?
    //    Compare h(f(x)) vs h(x).
    //    Heads equal. Arg 0: f(x) > x (Subterm).
    //    So h(f(x)) > h(x).
    // Conclusion: GREATER.
    auto lhs = h({ f({x}) });
    auto rhs = f({ h({x}) });

    EXPECT_TRUE(lpo.isGreater(lhs, rhs));
}

TEST_F(LpoTest, Recursion_SubtermTrumpsPrecedence) {
    // f < h.
    // Compare: f(a) vs h(f(a))
    // 1. Head Check: f < h.
    // 2. BUT: RHS contains LHS as a direct subterm.
    // Subterm rule is checked FIRST.
    // Result: LESS.
    auto sub = f({ a });
    auto super = h({ sub });

    EXPECT_TRUE(lpo.isLess(sub, super));
}

TEST_F(LpoTest, Recursion_NestedSubterm) {
    // f(g(h(x))) > h(x)
    // Transitive subterm check.
    auto deep = f({ g({h({x})}) });
    auto target = h({ x });

    // h(x) is a subterm of the argument of f.
    // f(...) > g(h(x)) > h(x).
    EXPECT_TRUE(lpo.isGreater(deep, target));
}

// ============================================================================
// GROUP 8: STABILITY & EDGE CASES
// Ensuring the order is partial and stable under substitution.
// ============================================================================

TEST_F(LpoTest, Stability_VariablePermutation_MustBeNC) {
    // f(x, y) vs f(y, x)
    // If we judged this GREATER or LESS, substituting x=a, y=b vs x=b, y=a 
    // would violate strict order properties.
    // Must be INCOMPARABLE.
    EXPECT_TRUE(lpo.isIncomparable(f({ x, y }), f({ y, x })));
}

TEST_F(LpoTest, Stability_VariableShielding) {
    // g > f (lexicographically). Same arity (1).
    // Compare: g(y) vs f(x)
    // 1. Head: g > f.
    // 2. Stability: Does g(y) > x (arg of RHS)?
    //    We don't know (NC).
    // Result: INCOMPARABLE.
    EXPECT_TRUE(lpo.isIncomparable(g({ y }), f({ x })));
}

TEST_F(LpoTest, EdgeCase_DifferentArity_SamePrefix) {
    // f(a, b) vs f(a)
    // 1. Arity 2 > Arity 1 (Precedence).
    // 2. Stability: f(a, b) > a (arg of RHS)? Yes.
    // Result: GREATER.
    EXPECT_TRUE(lpo.isGreater(f({ a, b }), f({ a })));
}

TEST_F(LpoTest, EdgeCase_EmptyFunctions) {
    // f() vs g() where g > f
    // Pure Precedence check.
    EXPECT_TRUE(lpo.isLess(f({}), g({})));
}

// ============================================================================
// GROUP 9: MIXED EXPRESSION TYPES
// Predicates vs Equality vs Functions
// ============================================================================

TEST_F(LpoTest, Mixed_Predicate_Vs_Equality) {
    // P(a) vs (a = a)
    // Predicate Type > Equality Type.
    auto pred = P({ a });
    auto eq = Equal(a, a);

    EXPECT_TRUE(lpo.isGreater(pred, eq));
}

TEST_F(LpoTest, Mixed_Equality_Vs_Function) {
    // (a = a) vs f(a, a)
    // Equality Type (3) > Function Type (2).
    // Note: Even though f(a,a) is "bigger" structurally, the Type Precedence 
    // defined in LPO (2.1) says Equality > Function.
    auto eq = Equal(a, a);
    auto func = f({ a, a });

    EXPECT_TRUE(lpo.isGreater(eq, func));
}

// ============================================================================
// GROUP 10: ADVANCED EQUALITY (Disjoint Sets & Logic)
// ============================================================================

TEST_F(LpoTest, Equality_DisjointSets_Dominance) {
    // LHS: (c = d) -> {c, d}. Assume c > d.
    // RHS: (a = b) -> {a, b}. Assume a > b.
    // Precedence: c > a > ...
    // Logic: No common elements.
    // c dominates both a and b. d dominates nothing, but c is enough.
    // Result: GREATER.
    auto eqLhs = Equal(c, b); // {c, b}
    auto eqRhs = Equal(a, a); // {a, a}

    EXPECT_TRUE(lpo.isGreater(eqLhs, eqRhs));
}

TEST_F(LpoTest, Equality_DisjointSets_Failure) {
    // LHS: (a = b) -> {a, b}
    // RHS: (c = d) -> {c, d}
    // c dominates a and b.
    // LHS cannot dominate RHS.
    // Result: LESS.
    auto eqLhs = Equal(a, b);
    auto eqRhs = Equal(c, b); // {c, b} common b removed -> {a} vs {c}

    EXPECT_TRUE(lpo.isLess(eqLhs, eqRhs));
}

TEST_F(LpoTest, Equality_Vs_Subterm) {
    // (f(a) = b) vs a
    // Multiset {f(a), b}.
    // f(a) > a (Subterm).
    // The whole expression dominates a.
    auto eq = Equal(f({ a }), b);

    EXPECT_TRUE(lpo.isGreater(eq, a));
}

// ============================================================================
// GROUP 11: PREDICATE INTERNALS (Arity & Lexicography)
// Verifies that Predicates follow standard LPO rules among themselves.
// ============================================================================

TEST_F(LpoTest, Predicate_Arity_Wins) {
    // P(a, b) [arity 2] vs P(a) [arity 1]
    // Even though names are equal, Arity 2 > Arity 1.
    auto pArity2 = P({ a, b });
    auto pArity1 = P({ a });

    EXPECT_TRUE(lpo.isGreater(pArity2, pArity1));
}

TEST_F(LpoTest, Predicate_Lexicography_Wins) {
    // P(b) vs P(a)
    // Same arity. Argument b > a.
    EXPECT_TRUE(lpo.isGreater(P({ b }), P({ a })));
}

TEST_F(LpoTest, Predicate_DifferentNames) {
    // Q(a) vs P(a)
    // "Q" > "P" lexicographically.
    EXPECT_TRUE(lpo.isGreater(Q({ a }), P({ a })));
}

// ============================================================================
// GROUP 12: DISTINCT OBJECTS (String Comparison Details)
// ============================================================================

TEST_F(LpoTest, Distinct_StringLexicography) {
    // In standard lexicographical order (ASCII):
    // "10" comes BEFORE "2".
    // So "2" > "10".
    auto d10 = Distinct("10");
    auto d2 = Distinct("2");

    EXPECT_TRUE(lpo.isGreater(d2, d10));
}

TEST_F(LpoTest, Distinct_LengthDoesNotMatterForStringCompare) {
    // "a" vs "aa"
    // "aa" > "a"
    auto da = Distinct("a");
    auto daa = Distinct("aa");

    EXPECT_TRUE(lpo.isGreater(daa, da));
}

// ============================================================================
// GROUP 13: DEEP STABILITY (NC Propagation & Shielding)
// Critical for ensuring the solver doesn't make false assumptions.
// ============================================================================

TEST_F(LpoTest, Stability_NC_Stops_LexicographicCheck) {
    // f(x, c) vs f(y, a)
    // Index 0: x vs y -> NC.
    // Index 1: c > a (Significant difference).
    // BUT: Since Index 0 is NC, we MUST NOT check Index 1.
    // The result must be NC.
    auto t1 = f({ x, c });
    auto t2 = f({ y, a });

    EXPECT_TRUE(lpo.isIncomparable(t1, t2));
}

TEST_F(LpoTest, Stability_VariableShielding_In_Precedence) {
    // g > f (lexicographically).
    // Compare: g(x) vs f(y)
    // 1. Head: g > f.
    // 2. Stability: Does g(x) > y (arg of RHS)?
    //    We don't know (NC).
    // Result: INCOMPARABLE.
    EXPECT_TRUE(lpo.isIncomparable(g({ x }), f({ y })));
}

// ============================================================================
// GROUP 14: FULL HIERARCHY CHAIN
// Predicate > Equality > Function > Distinct
// ============================================================================

TEST_F(LpoTest, Hierarchy_FullChainCheck) {
    auto pred = P({ a });
    auto eq = Equal(a, a);
    auto func = f({ a });
    auto dist = Distinct("heavy_name");

    // Check strict chain
    EXPECT_TRUE(lpo.isGreater(pred, eq));
    EXPECT_TRUE(lpo.isGreater(eq, func));
    EXPECT_TRUE(lpo.isGreater(func, dist));

    // Transitivity check (implicit)
    EXPECT_TRUE(lpo.isGreater(pred, dist));
}

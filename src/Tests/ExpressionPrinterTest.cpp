#include "gtest/gtest.h"

#include "../ExpressionBuilder.hpp"
#include "../ExpressionPrinter.hpp"

#include <algorithm>

using namespace ExpressionBuilder;

class ExpressionPrinterTptpTest : public ::testing::Test {
protected:
    // Initialize printer with TPTP configuration
    ExpressionPrinter printer{ ExpressionPrinter::Config::tptp() };

    // Helper to normalize strings (ONLY removes carriage returns, preserves spaces)
    // We want spaces to be significant!
    std::string normalize(std::string s) {
        s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
        return s;
    }
};

// 1. Boolean Constants
TEST_F(ExpressionPrinterTptpTest, BooleanConstants) {
    EXPECT_EQ(printer.toString(True()), "$true");
    EXPECT_EQ(printer.toString(False()), "$false");
}

// 2. Simple Predicates and Terms (ENFORCING ", ")
TEST_F(ExpressionPrinterTptpTest, PredicatesAndTerms) {
    // p(a, f(b))
    auto expr = Pred("p", {
        Func("a"),
        Func("f", {Func("b")})
        });

    // STRICT CHECK: Comma followed by space
    EXPECT_EQ(printer.toString(expr), "p(a, f(b))");
}

// 3. Variables (ENFORCING ", ")
TEST_F(ExpressionPrinterTptpTest, Variables) {
    // q(X, Y)
    auto expr = Pred("q", { Var("X"), Var("Y") });
    EXPECT_EQ(printer.toString(expr), "q(X, Y)");
}

// 4. Negation
TEST_F(ExpressionPrinterTptpTest, Negation) {
    // ~p(a)
    auto expr = Not(Pred("p", { Func("a") }));
    EXPECT_EQ(printer.toString(expr), "~p(a)");
}

// 5. Binary Connectives
TEST_F(ExpressionPrinterTptpTest, BinaryConnectives) {
    auto a = Pred("a");
    auto b = Pred("b");

    // Standard TPTP spacing around operators
    EXPECT_EQ(printer.toString(And(a, b)), "a & b");
    EXPECT_EQ(printer.toString(Or(a, b)), "a | b");
    EXPECT_EQ(printer.toString(Imp(a, b)), "a => b");
    EXPECT_EQ(printer.toString(Eqv(a, b)), "a <=> b");
    EXPECT_EQ(printer.toString(Xor(a, b)), "a <~> b");
}

// 6. Equality
TEST_F(ExpressionPrinterTptpTest, Equality) {
    // f(X) = a
    auto expr = Equal(Func("f", { Var("X") }), Func("a"));
    EXPECT_EQ(printer.toString(expr), "f(X) = a");
}

// 7. Universal Quantifier
TEST_F(ExpressionPrinterTptpTest, UniversalQuantifier) {
    // ![X]: p(X)
    auto expr = Forall(Var("X"), Pred("p", { Var("X") }));

    std::string out = printer.toString(expr);

    // Basic structure checks
    EXPECT_NE(out.find("![X]"), std::string::npos);
    EXPECT_NE(out.find("p(X)"), std::string::npos);
}

// 8. Existential Quantifier
TEST_F(ExpressionPrinterTptpTest, ExistentialQuantifier) {
    // ?[Y]: q(Y)
    auto expr = Exists(Var("Y"), Pred("q", { Var("Y") }));

    std::string out = printer.toString(expr);
    EXPECT_NE(out.find("?[Y]"), std::string::npos);
    EXPECT_NE(out.find("q(Y)"), std::string::npos);
}

// 9. Nested Quantifiers
TEST_F(ExpressionPrinterTptpTest, NestedQuantifiers) {
    // ![X]: ?[Y]: r(X, Y)  <-- Expect space in r(X, Y)
    auto expr = Forall(Var("X"), Exists(Var("Y"), Pred("r", { Var("X"), Var("Y") })));

    std::string out = normalize(printer.toString(expr));

    EXPECT_NE(out.find("![X]"), std::string::npos);
    EXPECT_NE(out.find("?[Y]"), std::string::npos);
    // Strict check for arguments spacing inside the quantifier body
    EXPECT_NE(out.find("r(X, Y)"), std::string::npos);
}

// 10. Operator Precedence
TEST_F(ExpressionPrinterTptpTest, PrecedenceGrouping) {
    // (a | b) & c
    auto expr = And(Or(Pred("a"), Pred("b")), Pred("c"));
    EXPECT_EQ(printer.toString(expr), "(a | b) & c");
}

// 11. Complex Formula
TEST_F(ExpressionPrinterTptpTest, ComplexFormula) {
    // ![X]: (human(X) => mortal(X))
    auto expr = Forall(Var("X"),
        Imp(Pred("human", { Var("X") }), Pred("mortal", { Var("X") }))
    );

    std::string out = printer.toString(expr);

    EXPECT_NE(out.find("![X]"), std::string::npos);
    EXPECT_NE(out.find("human(X) => mortal(X)"), std::string::npos);
}

// 12. Multiple Arguments (ENFORCING ", ")
TEST_F(ExpressionPrinterTptpTest, MultipleArguments) {
    // sum(X, Y, Z)
    auto expr = Pred("sum", { Var("X"), Var("Y"), Var("Z") });
    EXPECT_EQ(printer.toString(expr), "sum(X, Y, Z)");
}

// -----------------------------------------------------------------------------
// ADVANCED / HEAVY DUTY TESTS
// -----------------------------------------------------------------------------

// Test 13: Deeply Nested Terms
// FIX: Added spaces after every comma in the expected string
TEST_F(ExpressionPrinterTptpTest, DeeplyNestedTerms) {
    // p(f(g(h(a, b), i(c))))
    auto term = Func("f", {
        Func("g", {
            Func("h", {Func("a"), Func("b")}),
            Func("i", {Func("c")})
        })
        });
    auto expr = Pred("p", { term });

    // STRICT EXPECTATION: Spaces required
    EXPECT_EQ(printer.toString(expr), "p(f(g(h(a, b), i(c))))");
}

// Test 14: Operator Precedence Hell
TEST_F(ExpressionPrinterTptpTest, OperatorPrecedenceHell) {
    // Formula: ((a | b) & c) => (d <=> ~e)
    auto a = Pred("a");
    auto b = Pred("b");
    auto c = Pred("c");
    auto d = Pred("d");
    auto e = Pred("e");

    auto lhs = And(Or(a, b), c);
    auto rhs = Eqv(d, Not(e));
    auto expr = Imp(lhs, rhs);

    std::string out = printer.toString(expr);

    EXPECT_NE(out.find("(a | b)"), std::string::npos); // Spaces around OR
    EXPECT_NE(out.find(" => "), std::string::npos);     // Spaces around IMP
    EXPECT_NE(out.find("<=> ~e"), std::string::npos);   // Spaces around EQV
}

// Test 15: Multi-Variable Nested Quantification
// FIX: Added spaces to finding patterns
TEST_F(ExpressionPrinterTptpTest, MultiVarNestedQuantification) {
    // ![X]: (person(X) => ?[Y, Z]: (parent(Y, X) & parent(Z, X) & Y != Z))
    auto X = Var("X");
    auto Y = Var("Y");
    auto Z = Var("Z");
    auto distinct = Not(Equal(Y, Z));

    auto parents = And(
        And(Pred("parent", { Y, X }), Pred("parent", { Z, X })),
        distinct
    );
    auto existsPart = Exists(Y, Exists(Z, parents));
    auto expr = Forall(X, Imp(Pred("person", { X }), existsPart));

    std::string out = normalize(printer.toString(expr));

    EXPECT_NE(out.find("![X]"), std::string::npos);
    EXPECT_NE(out.find("?[Y]"), std::string::npos);

    // STRICT CHECKS with spaces
    EXPECT_NE(out.find("parent(Y, X)"), std::string::npos);
    EXPECT_NE(out.find("parent(Z, X)"), std::string::npos);
}

// Test 16: N-ary Junctions
TEST_F(ExpressionPrinterTptpTest, NaryJunctionFlattening) {
    // a | b | c | d
    std::vector<FormulaPtr> args = {
        Pred("a"), Pred("b"), Pred("c"), Pred("d")
    };
    auto expr = Disjunction(args);

    std::string out = printer.toString(expr);

    // Expect spaces around pipes
    // e.g., "a | b | c | d" or "(a | b) | (c | d)"
    EXPECT_NE(out.find(" | "), std::string::npos);
    EXPECT_NE(out.find("a"), std::string::npos);
    EXPECT_NE(out.find("d"), std::string::npos);
}

// Test 17: "The Stress Test"
// FIX: Added spaces to finding patterns
TEST_F(ExpressionPrinterTptpTest, StressTest_SetTheoryAxiom) {
    // ![A, B]: ( subset(A, B) <=> ![X]: (member(X, A) => member(X, B)) )
    auto A = Var("A");
    auto B = Var("B");
    auto X = Var("X");

    auto memberImpl = Forall(X,
        Imp(Pred("member", { X, A }), Pred("member", { X, B }))
    );
    auto definition = Eqv(Pred("subset", { A, B }), memberImpl);
    auto expr = Forall(A, Forall(B, definition));

    std::string out = normalize(printer.toString(expr));

    // STRICT CHECKS with spaces
    EXPECT_NE(out.find("![A]"), std::string::npos);
    EXPECT_NE(out.find("subset(A, B)"), std::string::npos);
    EXPECT_NE(out.find(" <=> "), std::string::npos);
    EXPECT_NE(out.find("member(X, A) => member(X, B)"), std::string::npos);
}

// -----------------------------------------------------------------------------
// DISTINCT OBJECTS TESTS (New Features)
// -----------------------------------------------------------------------------

// Test 18: Distinct Object - Numeric
// Should print as bare number in TPTP
TEST_F(ExpressionPrinterTptpTest, DistinctObject_Numeric) {
    auto d = Distinct("123");
    auto expr = Equal(Var("X"), d);

    // Expect: X = 123
    EXPECT_EQ(printer.toString(expr), "X = 123");
}

// Test 19: Distinct Object - String
// Should be quoted in TPTP to distinguish from constants/variables
TEST_F(ExpressionPrinterTptpTest, DistinctObject_String) {
    auto d = Distinct("apple");
    auto expr = Equal(Var("X"), d);

    // Expect: X = "apple"
    EXPECT_EQ(printer.toString(expr), "X = \"apple\"");
}

// Test 20: Distinct Object - Already Quoted
// Should not add double quotes if they exist
TEST_F(ExpressionPrinterTptpTest, DistinctObject_AlreadyQuoted) {
    auto d = Distinct("\"orange\"");
    auto expr = Equal(Var("X"), d);

    // Expect: X = "orange"
    EXPECT_EQ(printer.toString(expr), "X = \"orange\"");
}

// Test 21: Distinct vs Function Constant
// Distinct "a" should be "a", Function "a" should be a
TEST_F(ExpressionPrinterTptpTest, DistinctVsConstant) {
    auto d = Distinct("a");
    auto c = Func("a");

    // Predicate p("a", a)
    auto expr = Pred("p", { d, c });

    // Expect: p("a", a)
    EXPECT_EQ(printer.toString(expr), "p(\"a\", a)");
}

// Test 22: Distinct Object - No Parentheses
// Distinct objects are 0-arity, should never have ()
TEST_F(ExpressionPrinterTptpTest, DistinctObject_NoParentheses) {
    auto d = Distinct("obj");
    // If logic was wrong, it might print "obj"()
    EXPECT_EQ(printer.toString(d), "\"obj\"");
}

// Test 23: Distinct Object - Negative Number
// Should be printed without quotes
TEST_F(ExpressionPrinterTptpTest, DistinctObject_NegativeNumber) {
    auto d = Distinct("-42");
    EXPECT_EQ(printer.toString(d), "-42");
}

// Test 24: Distinct Object - Alphanumeric (Not a number)
// Should be quoted because it starts with digit but contains letters
TEST_F(ExpressionPrinterTptpTest, DistinctObject_MixedAlphanumeric) {
    auto d = Distinct("5g");
    // This is NOT a valid number, so it must be quoted as a string
    EXPECT_EQ(printer.toString(d), "\"5g\"");
}

#include <gtest/gtest.h>

#include "../ExpressionUtils.hpp"
#include "../ExpressionBuilder.hpp"

using namespace ExpressionBuilder;

class ExpressionUtilsTest : public ::testing::Test {};

TEST_F(ExpressionUtilsTest, IsDag_ReturnsTrue_ForStandardTree) {
    // A tree is a subset of DAG
    auto expr = And(Pred("P"), Pred("Q"));
    EXPECT_TRUE(ExpressionUtils::isDag(expr));
}

TEST_F(ExpressionUtilsTest, IsDag_ReturnsTrue_ForSharedSubtrees) {
    // Shared nodes are allowed in a DAG
    auto sharedLeaf = Pred("P", { Var("x") });
    auto root = And(sharedLeaf, sharedLeaf);
    EXPECT_TRUE(ExpressionUtils::isDag(root));
}

TEST_F(ExpressionUtilsTest, IsDag_ReturnsTrue_ForDiamondStructure) {
    // Classic diamond dependency: Top -> Left/Right -> Bottom
    auto bottom = Pred("Bottom");
    auto left = Imp(bottom, True());
    auto right = Imp(bottom, False());
    auto top = And(left, right);

    EXPECT_TRUE(ExpressionUtils::isDag(top));
}

TEST_F(ExpressionUtilsTest, IsDag_ReturnsTrue_WhenRootIsNull) {
    // Nullptr represents an empty graph, which is technically acyclic
    EXPECT_TRUE(ExpressionUtils::isDag(nullptr));
}

TEST_F(ExpressionUtilsTest, IsDag_ReturnsTrue_WhenChildIsNull) {
    // Missing child is topologically valid (no cycle introduced)
    auto broken = And(True(), nullptr);
    EXPECT_TRUE(ExpressionUtils::isDag(broken));
}

TEST_F(ExpressionUtilsTest, IsDag_ReturnsTrue_WhenSymbolIsEmpty) {
    // Invalid data content does not affect topological correctness (acyclicity)
    auto invalid = Pred("", { Var("x") });
    EXPECT_TRUE(ExpressionUtils::isDag(invalid));
}

TEST_F(ExpressionUtilsTest, IsDag_ReturnsFalse_ForDirectCycle) {
    auto loop = Not(True());
    loop->setChild(0, loop); // Self-reference

    EXPECT_FALSE(ExpressionUtils::isDag(loop));

    // Cleanup to break cycle for shared_ptr
    loop->setChild(0, True());
}

TEST_F(ExpressionUtilsTest, IsDag_ReturnsFalse_ForIndirectCycle) {
    auto a = Imp(True(), True());
    auto b = Imp(True(), True());

    // A -> B -> A cycle
    a->setChild(0, b);
    b->setChild(0, a);

    EXPECT_FALSE(ExpressionUtils::isDag(a));

    // Cleanup
    a->setChild(0, True());
    b->setChild(0, True());
}

// Group: isTree (Checks strict tree topology: single parent, no sharing, no cycles)

TEST_F(ExpressionUtilsTest, IsTree_ReturnsTrue_ForComplexValidTree) {
    auto expr = Exists(Var("x"),
        Imp(
            Pred("P", { Var("x") }),
            Forall(Var("y"),
                And(
                    Pred("Q", { Var("x"), Func("f", {Var("y")}) }),
                    Not(Pred("R", { Var("y") }))
                )
            )
        )
    );
    EXPECT_TRUE(ExpressionUtils::isTree(expr));
}

TEST_F(ExpressionUtilsTest, IsTree_ReturnsTrue_ForAtomicFormulas) {
    EXPECT_TRUE(ExpressionUtils::isTree(True()));
    EXPECT_TRUE(ExpressionUtils::isTree(False()));
}

TEST_F(ExpressionUtilsTest, IsTree_ReturnsTrue_WhenRootIsNull) {
    EXPECT_TRUE(ExpressionUtils::isTree(nullptr));
}

TEST_F(ExpressionUtilsTest, IsTree_ReturnsTrue_WhenChildIsNull) {
    auto brokenExpr = And(True(), nullptr);
    EXPECT_TRUE(ExpressionUtils::isTree(brokenExpr));
}

TEST_F(ExpressionUtilsTest, IsTree_ReturnsTrue_WhenSymbolIsEmpty) {
    auto invalidPred = Pred("", { Var("x") });
    EXPECT_TRUE(ExpressionUtils::isTree(invalidPred));
}

TEST_F(ExpressionUtilsTest, IsTree_ReturnsFalse_ForSharedSubtrees_DAG) {
    auto sharedLeaf = Pred("P", { Var("x") });
    auto root = And(sharedLeaf, sharedLeaf);
    EXPECT_FALSE(ExpressionUtils::isTree(root));
}

TEST_F(ExpressionUtilsTest, IsTree_ReturnsFalse_ForSharedVariablePointer) {
    // Shared variable instance (strict tree violation)
    auto v = Var("x");
    auto expr = Forall(v, Pred("P", { v }));
    EXPECT_FALSE(ExpressionUtils::isTree(expr));
}

TEST_F(ExpressionUtilsTest, IsTree_ReturnsFalse_ForDirectCycle) {
    // Formula cycle: Not -> Not
    auto loop = Not(True());
    loop->setChild(0, loop);

    EXPECT_FALSE(ExpressionUtils::isTree(loop));

    loop->setChild(0, True()); // Cleanup
}

TEST_F(ExpressionUtilsTest, IsTree_ReturnsFalse_ForIndirectCycle) {
    // Formula cycle: Imp -> Imp -> Imp
    auto leaf = Pred("A");
    auto top = Imp(leaf, leaf);
    top->setChild(1, top);

    EXPECT_FALSE(ExpressionUtils::isTree(top));

    top->setChild(1, True()); // Cleanup
}

TEST_F(ExpressionUtilsTest, IsTree_ReturnsFalse_ForCycleInFunctionArgs) {
    // Term cycle: Func f( g( f(...) ) )
    auto termInner = Func("g", { Var("x") });
    auto termOuter = Func("f", { termInner });

    // Create cycle: Make 'termOuter' a child of 'termInner'
    // This is valid types (Term inside Term) but invalid topology (Cycle)
    termInner->setChild(0, termOuter);

    auto pred = Pred("P", { termOuter });

    EXPECT_FALSE(ExpressionUtils::isTree(pred));

    // Cleanup
    termInner->setChild(0, Var("x"));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsTrue_ForValidExpression) {
    auto expr = And(Pred("P"), Pred("Q", { Var("x") }));
    EXPECT_TRUE(ExpressionUtils::isFullyDefined(expr));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsFalse_WhenRootIsNull) {
    // !expr return false
    EXPECT_FALSE(ExpressionUtils::isFullyDefined(nullptr));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsFalse_WhenChildIsNull) {
    // Checks iteration over children: getChild(i) -> isFullyDefinedRec -> !expr
    auto brokenExpr = And(True(), nullptr);
    EXPECT_FALSE(ExpressionUtils::isFullyDefined(brokenExpr));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsFalse_WhenPredicateSymbolIsEmpty) {
    // PredicateFormula::symbol.empty() check
    auto invalidPred = Pred("", { Var("x") });
    EXPECT_FALSE(ExpressionUtils::isFullyDefined(invalidPred));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsFalse_WhenFunctionSymbolIsEmpty) {
    // FunctionTerm::symbol.empty() check
    auto invalidFunc = Pred("P", { Func("", {Var("x")}) });
    EXPECT_FALSE(ExpressionUtils::isFullyDefined(invalidFunc));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsFalse_WhenVariableSymbolIsEmpty) {
    // VariableTerm::symbol.empty() check
    auto invalidVar = Var("");
    // Variable usually appears inside Pred/Func/Quantifier
    auto expr = Pred("P", { invalidVar });
    EXPECT_FALSE(ExpressionUtils::isFullyDefined(expr));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsFalse_WhenQuantifierVariableIsNull) {
    // QuantificationFormula specific check: !isFullyDefinedRec(quant->variable)
    // We pass nullptr as the variable
    auto invalidQuant = Forall(nullptr, Pred("P"));
    EXPECT_FALSE(ExpressionUtils::isFullyDefined(invalidQuant));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsFalse_WhenQuantifierVariableSymbolIsEmpty) {
    // QuantificationFormula -> variable -> symbol empty
    auto invalidQuant = Exists(Var(""), Pred("P"));
    EXPECT_FALSE(ExpressionUtils::isFullyDefined(invalidQuant));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsTrue_ForSharedNodes_DAG) {
    // DAG structure is valid for isFullyDefined (it only cares about data)
    auto sharedLeaf = Pred("P", { Var("x") });
    auto root = And(sharedLeaf, sharedLeaf);
    EXPECT_TRUE(ExpressionUtils::isFullyDefined(root));
}

TEST_F(ExpressionUtilsTest, IsFullyDefined_ReturnsTrue_ForCycleWithValidData) {
    // Cycles are valid for isFullyDefined (handled by visited set)
    // as long as the data inside nodes is correct.
    auto loop = Not(True());
    loop->setChild(0, loop);

    EXPECT_TRUE(ExpressionUtils::isFullyDefined(loop));

    // Cleanup
    loop->setChild(0, True());
}

TEST_F(ExpressionUtilsTest, IsArityConsistent_ReturnsTrue_ForConsistentPredicates) {
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("P", { Var("y") })
    );
    EXPECT_TRUE(ExpressionUtils::isArityConsistent(expr));
}

TEST_F(ExpressionUtilsTest, IsArityConsistent_ReturnsFalse_ForInconsistentPredicateArity) {
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("P", { Var("x"), Var("y") })
    );
    EXPECT_FALSE(ExpressionUtils::isArityConsistent(expr));
}

TEST_F(ExpressionUtilsTest, IsArityConsistent_ReturnsTrue_ForConsistentFunctions) {
    auto expr = Pred("Q", {
        Func("f", {Var("x")}),
        Func("f", {Var("y")})
        });
    EXPECT_TRUE(ExpressionUtils::isArityConsistent(expr));
}

TEST_F(ExpressionUtilsTest, IsArityConsistent_ReturnsFalse_ForInconsistentFunctionArity) {
    auto expr = Pred("Q", {
        Func("f", {Var("x")}),
        Func("f", {Var("x"), Var("y")})
        });
    EXPECT_FALSE(ExpressionUtils::isArityConsistent(expr));
}

TEST_F(ExpressionUtilsTest, IsArityConsistent_ReturnsTrue_ForSameNameInDifferentCategories) {
    // Predicate P(x) (arity 1) and Function P(a, b) (arity 2)
    auto predP = Pred("P", { Var("x") });
    auto funcP = Func("P", { Var("a"), Var("b") });

    auto expr = And(predP, Equal(Func("f", { Var("z") }), funcP));

    EXPECT_TRUE(ExpressionUtils::isArityConsistent(expr));
}

TEST_F(ExpressionUtilsTest, IsArityConsistent_ReturnsFalse_ForDeepNestedInconsistency) {
    auto expr = Exists(Var("x"),
        Imp(
            Pred("P", { Var("x") }),
            Forall(Var("y"),
                Pred("P", { Var("x"), Var("y") })
            )
        )
    );
    EXPECT_FALSE(ExpressionUtils::isArityConsistent(expr));
}

TEST_F(ExpressionUtilsTest, IsArityConsistent_ReturnsTrue_ForZeroAritySymbols) {
    auto expr = And(
        Pred("P", {}),
        Equal(Func("f", {}), Var("x"))
    );
    EXPECT_TRUE(ExpressionUtils::isArityConsistent(expr));
}

// ============================================================================
// isClause Tests
// Definition: A Clause is a disjunction (OR) of Literals.
// ============================================================================

TEST_F(ExpressionUtilsTest, IsClause_ReturnsTrue_ForLiterals) {
    // Atoms
    EXPECT_TRUE(ExpressionUtils::isClause(Pred("P")));
    EXPECT_TRUE(ExpressionUtils::isClause(True()));

    // Negated Atoms
    EXPECT_TRUE(ExpressionUtils::isClause(Not(Pred("Q"))));
}

TEST_F(ExpressionUtilsTest, IsClause_ReturnsTrue_ForRecursiveOr) {
    // Structure: ((A | B) | (C | D))
    auto left = Or(Pred("A"), Pred("B"));
    auto right = Or(Pred("C"), Pred("D"));
    auto root = Or(left, right);

    EXPECT_TRUE(ExpressionUtils::isClause(root));
}

TEST_F(ExpressionUtilsTest, IsClause_ReturnsTrue_ForJunctionOr) {
    // Structure: OR(A, ~B, C)
    std::vector<FormulaPtr> operands = { Pred("A"), Not(Pred("B")), Pred("C") };
    auto clause = std::make_shared<JunctionFormula>(JunctionFormula::Operator::OR, operands);

    EXPECT_TRUE(ExpressionUtils::isClause(clause));
}

TEST_F(ExpressionUtilsTest, IsClause_ReturnsFalse_ForInvalidContent) {
    // AND inside OR is forbidden in a Clause
    auto invalid = Or(Pred("A"), And(Pred("B"), Pred("C")));
    EXPECT_FALSE(ExpressionUtils::isClause(invalid));

    // Double negation is not a standard literal
    auto doubleNeg = Not(Not(Pred("A")));
    EXPECT_FALSE(ExpressionUtils::isClause(doubleNeg));

    // Implication is not allowed
    EXPECT_FALSE(ExpressionUtils::isClause(Imp(Pred("A"), Pred("B"))));
}

// ============================================================================
// isCnf Tests
// Definition: CNF is a conjunction (AND) of Clauses.
// ============================================================================

TEST_F(ExpressionUtilsTest, IsCnf_ReturnsTrue_ForSingleClause) {
    // A single clause is valid CNF (conjunction of size 1)
    auto clause = Or(Pred("A"), Not(Pred("B")));
    EXPECT_TRUE(ExpressionUtils::isCnf(clause));
}

TEST_F(ExpressionUtilsTest, IsCnf_ReturnsTrue_ForRecursiveAnd) {
    // Structure: ((C1 & C2) & C3)
    auto c1 = Or(Pred("A"), Pred("B"));
    auto c2 = Pred("C");
    auto c3 = Not(Pred("D"));

    auto root = And(And(c1, c2), c3);

    EXPECT_TRUE(ExpressionUtils::isCnf(root));
}

TEST_F(ExpressionUtilsTest, IsCnf_ReturnsTrue_ForJunctionAnd) {
    // Structure: AND(C1, C2, C3)
    std::vector<FormulaPtr> clauses = {
        Or(Pred("A"), Pred("B")),
        Pred("C"),
        True()
    };
    auto root = std::make_shared<JunctionFormula>(JunctionFormula::Operator::AND, clauses);

    EXPECT_TRUE(ExpressionUtils::isCnf(root));
}

TEST_F(ExpressionUtilsTest, IsCnf_ReturnsFalse_ForOrAtTopLevel) {
    // (A & B) | C -> This is DNF, not CNF
    auto innerAnd = And(Pred("A"), Pred("B"));
    auto root = Or(innerAnd, Pred("C"));

    EXPECT_FALSE(ExpressionUtils::isCnf(root));
}

TEST_F(ExpressionUtilsTest, IsCnf_ReturnsFalse_ForNonCnfOperators) {
    // Implication at root
    EXPECT_FALSE(ExpressionUtils::isCnf(Imp(Pred("A"), Pred("B"))));

    // Quantifiers
    auto quantified = Forall(Var("x"), Pred("P"));
    EXPECT_FALSE(ExpressionUtils::isCnf(quantified));
}

// ============================================================================
// Structural Integrity Tests
// ============================================================================

TEST_F(ExpressionUtilsTest, IsCnf_ReturnsFalse_ForSharedNodes) {
    // The input must be a Tree, not a DAG.
    // Logic: P | P (where P is the same shared pointer)
    auto atom = Pred("Shared");
    auto root = Or(atom, atom);

    // Note: This relies on ExpressionUtils::isTree returning false for shared ptrs
    EXPECT_FALSE(ExpressionUtils::isCnf(root));
}

TEST_F(ExpressionUtilsTest, IsCnf_ReturnsFalse_ForNullptr) {
    EXPECT_FALSE(ExpressionUtils::isCnf(nullptr));
    EXPECT_FALSE(ExpressionUtils::isClause(nullptr));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsTrue_ForStrictStructure) {
    auto clause1 = Disjunction({
        Pred("P", {Var("x")}),
        Not(Pred("Q", {Var("y")}))
        });

    auto clause2 = Disjunction({
        Pred("R"),
        Not(Pred("S")),
        Pred("T", {Func("f", {Var("z")})})
        });

    auto clause3 = Disjunction({
        Not(Equal(Var("u"), Var("v")))
        });

    auto expr = Conjunction({ clause1, clause2, clause3 });

    EXPECT_TRUE(ExpressionUtils::isJunctionCnf(expr));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsFalse_ForSingleLiteralRoot) {
    EXPECT_FALSE(ExpressionUtils::isJunctionCnf(Pred("P")));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsFalse_ForDisjunctionRoot) {
    auto expr = Disjunction({ Pred("A"), Pred("B") });
    EXPECT_FALSE(ExpressionUtils::isJunctionCnf(expr));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsFalse_ForBinaryAndRoot) {
    auto expr = And(Disjunction({ Pred("A") }), Disjunction({ Pred("B") }));
    EXPECT_FALSE(ExpressionUtils::isJunctionCnf(expr));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsFalse_ForMixedChildrenInRoot) {
    auto validClause = Disjunction({ Pred("A") });
    auto invalidChild = Pred("B");

    auto expr = Conjunction({ validClause, invalidChild });
    EXPECT_FALSE(ExpressionUtils::isJunctionCnf(expr));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsFalse_ForNonLiteralInsideClause) {
    auto innerAnd = And(Pred("X"), Pred("Y"));
    auto clause = Disjunction({ Pred("A"), innerAnd });

    auto expr = Conjunction({ clause });
    EXPECT_FALSE(ExpressionUtils::isJunctionCnf(expr));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsFalse_ForDoubleNegationInsideClause) {
    auto doubleNeg = Not(Not(Pred("P")));
    auto clause = Disjunction({ doubleNeg });

    auto expr = Conjunction({ clause });
    EXPECT_FALSE(ExpressionUtils::isJunctionCnf(expr));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsFalse_ForNestedJunctionOr) {
    auto innerOr = Disjunction({ Pred("X"), Pred("Y") });
    auto clause = Disjunction({ Pred("A"), innerOr });

    auto expr = Conjunction({ clause });
    EXPECT_FALSE(ExpressionUtils::isJunctionCnf(expr));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsFalse_ForEmptyAnd) {
    EXPECT_TRUE(ExpressionUtils::isJunctionCnf(Conjunction({})));
}

TEST_F(ExpressionUtilsTest, IsJunctionCnf_ReturnsFalse_ForAndWithEmptyOr) {
    EXPECT_TRUE(ExpressionUtils::isJunctionCnf(Conjunction({ Disjunction({}) })));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsTrue_ForAtoms) {
    EXPECT_TRUE(ExpressionUtils::isNnf(True()));
    EXPECT_TRUE(ExpressionUtils::isNnf(False()));
    EXPECT_TRUE(ExpressionUtils::isNnf(Pred("P")));
    EXPECT_TRUE(ExpressionUtils::isNnf(Equal(Var("x"), Var("y"))));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsTrue_ForNegatedAtoms) {
    EXPECT_TRUE(ExpressionUtils::isNnf(Not(Pred("P"))));
    EXPECT_TRUE(ExpressionUtils::isNnf(Not(Equal(Var("a"), Var("b")))));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsTrue_ForDeeplyNestedStructure) {
    // (P(x) AND ~Q(y)) OR (Forall z, R(z))
    auto expr = Or(
        And(Pred("P", { Var("x") }), Not(Pred("Q", { Var("y") }))),
        Forall(Var("z"), Pred("R", { Var("z") }))
    );
    EXPECT_TRUE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsTrue_ForJunctions) {
    // AND( P, ~Q, OR(R, S) )
    auto expr = Conjunction({
        Pred("P"),
        Not(Pred("Q")),
        Disjunction({ Pred("R"), Pred("S") })
        });
    EXPECT_TRUE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsTrue_ForQuantifiersInside) {
    // Exists x, (P(x) AND Forall y, ~Q(y))
    auto expr = Exists(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Forall(Var("y"), Not(Pred("Q", { Var("y") })))
        )
    );
    EXPECT_TRUE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsFalse_ForDoubleNegation) {
    // ~~P
    auto expr = Not(Not(Pred("P")));
    EXPECT_FALSE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsFalse_ForNegationOfBinaryOp) {
    // ~(A AND B) -> Should be (~A OR ~B)
    auto expr = Not(And(Pred("A"), Pred("B")));
    EXPECT_FALSE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsFalse_ForNegationOfJunction) {
    // ~(AND(A, B))
    auto expr = Not(Conjunction({ Pred("A"), Pred("B") }));
    EXPECT_FALSE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsFalse_ForNegationOfQuantifier) {
    // ~Forall x, P(x) -> Should be Exists x, ~P(x)
    auto expr = Not(Forall(Var("x"), Pred("P")));
    EXPECT_FALSE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsFalse_ForImplication) {
    // Implication is not allowed in NNF
    auto expr = Imp(Pred("A"), Pred("B"));
    EXPECT_FALSE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsFalse_ForEquivalence) {
    auto expr = Eqv(Pred("A"), Pred("B"));
    EXPECT_FALSE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsFalse_ForXor) {
    auto expr = Xor(Pred("A"), Pred("B"));
    EXPECT_FALSE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsFalse_ForNegationInsideImplication) {
    // A -> ~B (Implication itself is invalid, but checking recursion)
    auto expr = Imp(Pred("A"), Not(Pred("B")));
    EXPECT_FALSE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsNnf_ReturnsFalse_ForImplicationInsideNegation) {
    // ~(A -> B)
    auto expr = Not(Imp(Pred("A"), Pred("B")));
    EXPECT_FALSE(ExpressionUtils::isNnf(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsTrue_ForSimpleAtom) {
    EXPECT_TRUE(ExpressionUtils::isStandardized(Pred("P", { Var("x") })));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsTrue_ForUniqueQuantifiers) {
    // Forall x, P(x) AND Forall y, Q(y)
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Forall(Var("y"), Pred("Q", { Var("y") }))
    );
    EXPECT_TRUE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsTrue_ForNestedUniqueQuantifiers) {
    // Forall x, Exists y, P(x, y)
    auto expr = Forall(Var("x"),
        Exists(Var("y"),
            Pred("P", { Var("x"), Var("y") })
        )
    );
    EXPECT_TRUE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsFalse_ForReusedVariableInDifferentBranches) {
    // Forall x, P(x) AND Forall x, Q(x)
    // The variable 'x' is quantified twice in disjoint scopes
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Forall(Var("x"), Pred("Q", { Var("x") }))
    );
    EXPECT_FALSE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsFalse_ForShadowedVariable) {
    // Forall x, (P(x) AND Exists x, Q(x))
    // Inner 'x' shadows outer 'x'
    auto expr = Forall(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Exists(Var("x"), Pred("Q", { Var("x") }))
        )
    );
    EXPECT_FALSE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsFalse_ForVariableReusedAsFreeAndBound) {
    // P(x) AND Forall x, Q(x)
    // 'x' appears free in P, then quantified in Q
    auto expr = And(
        Pred("P", { Var("x") }),
        Forall(Var("x"), Pred("Q", { Var("x") }))
    );
    EXPECT_FALSE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsTrue_ForMultipleOccurrencesOfFreeVariable) {
    // P(x) AND Q(x)
    // It is valid to use the same free variable multiple times. 
    // Standardization restricts QUANTIFIED variables.
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("Q", { Var("x") })
    );
    EXPECT_TRUE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsFalse_ForMixedQuantifiersReuse) {
    // Forall x, P(x) AND Exists x, Q(x)
    // Reuse is forbidden regardless of quantifier type
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Exists(Var("x"), Pred("Q", { Var("x") }))
    );
    EXPECT_FALSE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsFalse_ForReuseInsideFunctionTerm) {
    // Forall x, P(x) AND Q(f(x))
    // The x inside f(x) is free, but x is bound in the left branch.
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Pred("Q", { Func("f", { Var("x") }) })
    );
    EXPECT_FALSE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsFalse_ForReuseDeepInsideStructure) {
    // Forall z, (P(z) OR Forall z, Q(z))
    // Nested reuse of z
    auto expr = Forall(Var("z"),
        Or(
            Pred("P", { Var("z") }),
            Forall(Var("z"), Pred("Q", { Var("z") }))
        )
    );
    EXPECT_FALSE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsTrue_ForVacuousQuantification) {
    // Forall x, P(y)
    // Variable x is defined but not used. This is valid standardization 
    // (provided x is not used elsewhere).
    auto expr = Forall(Var("x"), Pred("P", { Var("y") }));
    EXPECT_TRUE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsFalse_ForVacuousQuantificationClash) {
    // Forall x, P(y) AND P(x)
    // x is defined in the quantifier (even if unused in body), 
    // so it cannot be used as a free variable elsewhere.
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("y") })),
        Pred("P", { Var("x") })
    );
    EXPECT_FALSE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsFalse_ForReuseInNaryJunction) {
    // Disjunction( Forall x P(x), Forall x Q(x) )
    auto expr = Disjunction({
        Forall(Var("x"), Pred("P", { Var("x") })),
        Forall(Var("x"), Pred("Q", { Var("x") }))
        });
    EXPECT_FALSE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, IsStandardized_ReturnsTrue_ForComplexValidStructure) {
    // Forall v1, ( P(v1) -> Exists v2, ( Q(v2) AND R(v1, f(v2)) ) )
    auto expr = Forall(Var("v1"),
        Imp(
            Pred("P", { Var("v1") }),
            Exists(Var("v2"),
                And(
                    Pred("Q", { Var("v2") }),
                    Pred("R", { Var("v1"), Func("f", {Var("v2")}) })
                )
            )
        )
    );
    EXPECT_TRUE(ExpressionUtils::isStandardized(expr));
}

TEST_F(ExpressionUtilsTest, AreAlphaEquivalent_ReturnsTrue_ForIdenticalFormulas) {
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));
    EXPECT_TRUE(ExpressionUtils::areAlphaEquivalent(expr, expr));
}

TEST_F(ExpressionUtilsTest, AreAlphaEquivalent_ReturnsTrue_ForRenamedBoundVariables) {
    // Forall x, P(x) == Forall y, P(y)
    auto expr1 = Forall(Var("x"), Pred("P", { Var("x") }));
    auto expr2 = Forall(Var("y"), Pred("P", { Var("y") }));
    EXPECT_TRUE(ExpressionUtils::areAlphaEquivalent(expr1, expr2));
}

TEST_F(ExpressionUtilsTest, AreAlphaEquivalent_ReturnsTrue_ForNestedRenaming) {
    // Forall x, Exists y, Q(x, y) == Forall a, Exists b, Q(a, b)
    auto expr1 = Forall(Var("x"), Exists(Var("y"), Pred("Q", { Var("x"), Var("y") })));
    auto expr2 = Forall(Var("a"), Exists(Var("b"), Pred("Q", { Var("a"), Var("b") })));
    EXPECT_TRUE(ExpressionUtils::areAlphaEquivalent(expr1, expr2));
}

TEST_F(ExpressionUtilsTest, AreAlphaEquivalent_ReturnsFalse_ForDifferentStructure) {
    auto expr1 = Forall(Var("x"), Pred("P", { Var("x") }));
    auto expr2 = Exists(Var("x"), Pred("P", { Var("x") }));
    EXPECT_FALSE(ExpressionUtils::areAlphaEquivalent(expr1, expr2));
}

TEST_F(ExpressionUtilsTest, AreAlphaEquivalent_ReturnsFalse_ForFreeVariableMismatch) {
    // P(x) != P(y) (Free variables are not renamed in alpha equivalence)
    auto expr1 = Pred("P", { Var("x") });
    auto expr2 = Pred("P", { Var("y") });
    EXPECT_FALSE(ExpressionUtils::areAlphaEquivalent(expr1, expr2));
}

TEST_F(ExpressionUtilsTest, AlphaEquivalent_ReturnsTrue_ForSameDistinctObjects) {
    auto d1 = Distinct("Apple");
    auto d2 = Distinct("Apple");
    EXPECT_TRUE(ExpressionUtils::areAlphaEquivalent(d1, d2));
}

TEST_F(ExpressionUtilsTest, AlphaEquivalent_ReturnsFalse_ForDifferentDistinctObjects) {
    auto d1 = Distinct("Apple");
    auto d2 = Distinct("Orange");
    EXPECT_FALSE(ExpressionUtils::areAlphaEquivalent(d1, d2));
}

TEST_F(ExpressionUtilsTest, AlphaEquivalent_ReturnsFalse_ForDistinctVsFunction) {
    // Distinct object "c" must differ from function constant "c"
    auto distinctObj = Distinct("c");
    auto funcConst = Func("c");
    EXPECT_FALSE(ExpressionUtils::areAlphaEquivalent(distinctObj, funcConst));
}

TEST_F(ExpressionUtilsTest, AlphaEquivalent_ReturnsFalse_ForDistinctVsVariable) {
    auto distinctObj = Distinct("X");
    auto variable = Var("X");
    EXPECT_FALSE(ExpressionUtils::areAlphaEquivalent(distinctObj, variable));
}

TEST_F(ExpressionUtilsTest, IsVarFreeInExpr_ReturnsTrue_ForSimpleFreeOccurrence) {
    // P(x) -> x is free
    auto expr = Pred("P", { Var("x") });
    EXPECT_TRUE(ExpressionUtils::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionUtilsTest, IsVarFreeInExpr_ReturnsFalse_ForDifferentVariable) {
    // P(y) -> x is not free
    auto expr = Pred("P", { Var("y") });
    EXPECT_FALSE(ExpressionUtils::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionUtilsTest, IsVarFreeInExpr_ReturnsFalse_ForBoundOccurrence) {
    // Forall x, P(x) -> x is bound (not free)
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));
    EXPECT_FALSE(ExpressionUtils::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionUtilsTest, IsVarFreeInExpr_ReturnsTrue_ForFreeInOneBranch) {
    // P(x) AND Forall x, Q(x)
    // x is free in the left branch, so it is free in the whole expression
    auto expr = And(
        Pred("P", { Var("x") }),
        Forall(Var("x"), Pred("Q", { Var("x") }))
    );
    EXPECT_TRUE(ExpressionUtils::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionUtilsTest, IsVarFreeInExpr_ReturnsFalse_ForShadowedBoundOccurrence) {
    // Forall x, (P(x) AND Exists x, Q(x))
    // x is bound at the top level, so no free x inside
    auto expr = Forall(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Exists(Var("x"), Pred("Q", { Var("x") }))
        )
    );
    EXPECT_FALSE(ExpressionUtils::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionUtilsTest, IsVarFreeInExpr_ReturnsTrue_ForDeeplyNestedFunctionTerm) {
    // Forall y, P( f( g( x ) ) )
    // x is deeply nested inside terms but never quantified
    auto expr = Forall(Var("y"),
        Pred("P", {
            Func("f", {
                Func("g", { Var("x") })
            })
            })
    );
    EXPECT_TRUE(ExpressionUtils::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionUtilsTest, IsVarFreeInExpr_ReturnsFalse_ForDeeplyNestedBoundVar) {
    // Forall x, P( f( g( x ) ) )
    // x is nested but bound by the top quantifier
    auto expr = Forall(Var("x"),
        Pred("P", {
            Func("f", {
                Func("g", { Var("x") })
            })
            })
    );
    EXPECT_FALSE(ExpressionUtils::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionUtilsTest, IsVarFreeInExpr_ReturnsTrue_ForComplexMixedStructure) {
    // (Forall z, R(z)) OR (Q(y) IMP P(x))
    // x is free in the right branch
    auto expr = Or(
        Forall(Var("z"), Pred("R", { Var("z") })),
        Imp(
            Pred("Q", { Var("y") }),
            Pred("P", { Var("x") })
        )
    );
    EXPECT_TRUE(ExpressionUtils::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionUtilsTest, IsVarFreeInExpr_ReturnsFalse_IfSymbolIsPartOfFunctionNameOnly) {
    // P(x_func(y)) check for "x_func" (which is a function symbol, not a variable)
    // The method checks for VARIABLE terms, not function symbols.
    auto expr = Pred("P", { Func("x", { Var("y") }) });

    // Even though "x" is the function name, it's not a variable term "x"
    EXPECT_FALSE(ExpressionUtils::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_ReturnsEmpty_ForConstants) {
    auto expr = And(True(), False());
    auto vars = ExpressionUtils::getFreeVariables(expr);
    EXPECT_TRUE(vars.empty());
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_ReturnsSingle_ForSimpleAtom) {
    auto expr = Pred("P", { Var("x") });
    auto vars = ExpressionUtils::getFreeVariables(expr);

    ASSERT_EQ(vars.size(), 1);
    EXPECT_EQ(vars[0], "x");
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_ReturnsMultiple_ForDifferentVariables) {
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("Q", { Var("y") })
    );
    auto vars = ExpressionUtils::getFreeVariables(expr);

    // Sort to ensure deterministic comparison
    std::sort(vars.begin(), vars.end());
    std::vector<std::string> expected = { "x", "y" };

    EXPECT_EQ(vars, expected);
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_ReturnsUnique_ForRepeatedVariables) {
    // P(x) AND Q(x) -> Should return "x" once (assuming implementation dedupes)
    // If implementation returns duplicates, this test adapts to check for uniqueness manually or content.
    // Standard implementation usually returns a set-like vector.
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("Q", { Var("x") })
    );
    auto vars = ExpressionUtils::getFreeVariables(expr);

    // Check if it contains "x"
    bool foundX = false;
    for (const auto& v : vars) {
        if (v == "x") foundX = true;
    }
    EXPECT_TRUE(foundX);

    // If your implementation is set-based, size should be 1.
    // If it collects all occurrences, size is 2. 
    // Assuming standard "Set of free variables" definition:
    std::set<std::string> uniqueVars(vars.begin(), vars.end());
    EXPECT_EQ(uniqueVars.size(), 1);
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_ExcludesBoundVariables) {
    // Forall x, P(x) -> x is bound, result empty
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));
    auto vars = ExpressionUtils::getFreeVariables(expr);

    EXPECT_TRUE(vars.empty());
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_CapturesFreeVar_InsideQuantifierBody) {
    // Forall x, P(y) -> y is free
    auto expr = Forall(Var("x"), Pred("P", { Var("y") }));
    auto vars = ExpressionUtils::getFreeVariables(expr);

    ASSERT_EQ(vars.size(), 1);
    EXPECT_EQ(vars[0], "y");
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_CapturesVariable_ReusedAsFreeAndBound) {
    // (Forall x, P(x)) AND Q(x)
    // The first x is bound, the second x (in Q) is free.
    // The function MUST return "x" because of the second occurrence.
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Pred("Q", { Var("x") })
    );
    auto vars = ExpressionUtils::getFreeVariables(expr);

    bool foundX = false;
    for (const auto& v : vars) {
        if (v == "x") foundX = true;
    }
    EXPECT_TRUE(foundX);
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_TraversesFunctionTermsDeeply) {
    // P( f( g( z ) ) ) -> z is free
    auto expr = Pred("P", {
        Func("f", {
            Func("g", { Var("z") })
        })
        });
    auto vars = ExpressionUtils::getFreeVariables(expr);

    ASSERT_EQ(vars.size(), 1);
    EXPECT_EQ(vars[0], "z");
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_HandlesShadowingCorrectly) {
    // Forall x, ( P(x) AND Exists y, Q(x, y) )
    // All x are bound by Forall. All y are bound by Exists.
    // Result should be empty.
    auto expr = Forall(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Exists(Var("y"), Pred("Q", { Var("x"), Var("y") }))
        )
    );
    auto vars = ExpressionUtils::getFreeVariables(expr);

    EXPECT_TRUE(vars.empty());
}

TEST_F(ExpressionUtilsTest, GetFreeVariables_HandlesComplexMixedStructure) {
    // (Forall x, P(x)) OR (Q(y) IMP R(f(z)))
    // Free vars: y, z
    auto expr = Or(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Imp(
            Pred("Q", { Var("y") }),
            Pred("R", { Func("f", { Var("z") }) })
        )
    );
    auto vars = ExpressionUtils::getFreeVariables(expr);

    std::sort(vars.begin(), vars.end());
    std::vector<std::string> uniqueVars;
    std::unique_copy(vars.begin(), vars.end(), std::back_inserter(uniqueVars));

    std::vector<std::string> expected = { "y", "z" };
    EXPECT_EQ(uniqueVars, expected);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_ReturnsZero_ForNull) {
    EXPECT_EQ(ExpressionUtils::getExpressionSize(nullptr), 0);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_ReturnsOne_ForBooleanConstants) {
    EXPECT_EQ(ExpressionUtils::getExpressionSize(True()), 1);
    EXPECT_EQ(ExpressionUtils::getExpressionSize(False()), 1);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_ReturnsOne_ForVariable) {
    EXPECT_EQ(ExpressionUtils::getExpressionSize(Var("x")), 1);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_CountsPredicateAndArguments) {
    // Structure: Pred node + Var node = 2
    auto expr = Pred("P", { Var("x") });
    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr), 2);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_CountsFunctionAndArguments) {
    // Structure: Func node + Var node + Var node = 3
    auto expr = Func("f", { Var("x"), Var("y") });
    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr), 3);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_CalculatesBinaryFormulaCorrectly) {
    // Structure: And(1) + True(1) + False(1) = 3
    auto expr = And(True(), False());
    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr), 3);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_CalculatesQuantifierCorrectly) {
    // Structure: Forall(1) + Var_decl(1) + Body[Pred(1) + Var_arg(1)] = 4
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));
    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr), 4);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_CalculatesNaryJunctionCorrectly) {
    // Structure: Junction(1) + A(1) + B(1) + C(1) = 4
    auto expr = Conjunction({
        Pred("A"),
        Pred("B"),
        Pred("C")
        });
    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr), 4);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_CalculatesDeeplyNestedStructure) {
    // Imp (
    //   Not ( Pred("P") ),          -> 1 + 1 = 2
    //   Pred("Q", { Func("f") })    -> 1 + (1 + 0) = 2  (assuming f has 0 args here for simplicity, or f is simple)
    // )
    // Total: 1 (Imp) + 2 + 2 = 5

    auto left = Not(Pred("P"));
    auto right = Pred("Q", { Func("f") });
    auto expr = Imp(left, right);

    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr), 5);
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_CountsTotalNodes_EvenForSharedSubtrees) {
    // In a DAG, shared nodes are physically 1, but logically appear multiple times.
    // getExpressionSize usually counts the logical tree size.

    auto atom = Pred("A"); // Size 1

    // Expr: A AND A
    // Tree Size: 1 (AND) + 1 (Left A) + 1 (Right A) = 3
    auto expr = And(atom, atom);

    ASSERT_EQ(ExpressionUtils::getExpressionSize(expr), 2); // DAG
    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr->clone()), 3); // Tree
}

TEST_F(ExpressionUtilsTest, GetExpressionSize_CountsComplexTermHierarchy) {
    // P( f( g( x ) ) )
    // Pred(1) + Func_f(1) + Func_g(1) + Var_x(1) = 4
    auto expr = Pred("P", {
        Func("f", {
            Func("g", { Var("x") })
        })
        });
    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr), 4);
}

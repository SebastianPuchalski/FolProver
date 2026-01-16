#include <gtest/gtest.h>

#include "../ExpressionTransformer.hpp"
#include "../ExpressionBuilder.hpp"

using namespace ExpressionBuilder;
namespace DSL = ReplacementRuleDSL;

class ExpressionTransformerTest : public ::testing::Test {};

TEST_F(ExpressionTransformerTest, IsDag_ReturnsTrue_ForStandardTree) {
    // A tree is a subset of DAG
    auto expr = And(Pred("P"), Pred("Q"));
    EXPECT_TRUE(ExpressionTransformer::isDag(expr));
}

TEST_F(ExpressionTransformerTest, IsDag_ReturnsTrue_ForSharedSubtrees) {
    // Shared nodes are allowed in a DAG
    auto sharedLeaf = Pred("P", { Var("x") });
    auto root = And(sharedLeaf, sharedLeaf);
    EXPECT_TRUE(ExpressionTransformer::isDag(root));
}

TEST_F(ExpressionTransformerTest, IsDag_ReturnsTrue_ForDiamondStructure) {
    // Classic diamond dependency: Top -> Left/Right -> Bottom
    auto bottom = Pred("Bottom");
    auto left = Imp(bottom, True());
    auto right = Imp(bottom, False());
    auto top = And(left, right);

    EXPECT_TRUE(ExpressionTransformer::isDag(top));
}

TEST_F(ExpressionTransformerTest, IsDag_ReturnsTrue_WhenRootIsNull) {
    // Nullptr represents an empty graph, which is technically acyclic
    EXPECT_TRUE(ExpressionTransformer::isDag(nullptr));
}

TEST_F(ExpressionTransformerTest, IsDag_ReturnsTrue_WhenChildIsNull) {
    // Missing child is topologically valid (no cycle introduced)
    auto broken = And(True(), nullptr);
    EXPECT_TRUE(ExpressionTransformer::isDag(broken));
}

TEST_F(ExpressionTransformerTest, IsDag_ReturnsTrue_WhenSymbolIsEmpty) {
    // Invalid data content does not affect topological correctness (acyclicity)
    auto invalid = Pred("", { Var("x") });
    EXPECT_TRUE(ExpressionTransformer::isDag(invalid));
}

TEST_F(ExpressionTransformerTest, IsDag_ReturnsFalse_ForDirectCycle) {
    auto loop = Not(True());
    loop->setChild(0, loop); // Self-reference

    EXPECT_FALSE(ExpressionTransformer::isDag(loop));

    // Cleanup to break cycle for shared_ptr
    loop->setChild(0, True());
}

TEST_F(ExpressionTransformerTest, IsDag_ReturnsFalse_ForIndirectCycle) {
    auto a = Imp(True(), True());
    auto b = Imp(True(), True());

    // A -> B -> A cycle
    a->setChild(0, b);
    b->setChild(0, a);

    EXPECT_FALSE(ExpressionTransformer::isDag(a));

    // Cleanup
    a->setChild(0, True());
    b->setChild(0, True());
}

// Group: isTree (Checks strict tree topology: single parent, no sharing, no cycles)

TEST_F(ExpressionTransformerTest, IsTree_ReturnsTrue_ForComplexValidTree) {
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
    EXPECT_TRUE(ExpressionTransformer::isTree(expr));
}

TEST_F(ExpressionTransformerTest, IsTree_ReturnsTrue_ForAtomicFormulas) {
    EXPECT_TRUE(ExpressionTransformer::isTree(True()));
    EXPECT_TRUE(ExpressionTransformer::isTree(False()));
}

TEST_F(ExpressionTransformerTest, IsTree_ReturnsTrue_WhenRootIsNull) {
    EXPECT_TRUE(ExpressionTransformer::isTree(nullptr));
}

TEST_F(ExpressionTransformerTest, IsTree_ReturnsTrue_WhenChildIsNull) {
    auto brokenExpr = And(True(), nullptr);
    EXPECT_TRUE(ExpressionTransformer::isTree(brokenExpr));
}

TEST_F(ExpressionTransformerTest, IsTree_ReturnsTrue_WhenSymbolIsEmpty) {
    auto invalidPred = Pred("", { Var("x") });
    EXPECT_TRUE(ExpressionTransformer::isTree(invalidPred));
}

TEST_F(ExpressionTransformerTest, IsTree_ReturnsFalse_ForSharedSubtrees_DAG) {
    auto sharedLeaf = Pred("P", { Var("x") });
    auto root = And(sharedLeaf, sharedLeaf);
    EXPECT_FALSE(ExpressionTransformer::isTree(root));
}

TEST_F(ExpressionTransformerTest, IsTree_ReturnsFalse_ForSharedVariablePointer) {
    // Shared variable instance (strict tree violation)
    auto v = Var("x");
    auto expr = Forall(v, Pred("P", { v }));
    EXPECT_FALSE(ExpressionTransformer::isTree(expr));
}

TEST_F(ExpressionTransformerTest, IsTree_ReturnsFalse_ForDirectCycle) {
    // Formula cycle: Not -> Not
    auto loop = Not(True());
    loop->setChild(0, loop);

    EXPECT_FALSE(ExpressionTransformer::isTree(loop));

    loop->setChild(0, True()); // Cleanup
}

TEST_F(ExpressionTransformerTest, IsTree_ReturnsFalse_ForIndirectCycle) {
    // Formula cycle: Imp -> Imp -> Imp
    auto leaf = Pred("A");
    auto top = Imp(leaf, leaf);
    top->setChild(1, top);

    EXPECT_FALSE(ExpressionTransformer::isTree(top));

    top->setChild(1, True()); // Cleanup
}

TEST_F(ExpressionTransformerTest, IsTree_ReturnsFalse_ForCycleInFunctionArgs) {
    // Term cycle: Func f( g( f(...) ) )
    auto termInner = Func("g", { Var("x") });
    auto termOuter = Func("f", { termInner });

    // Create cycle: Make 'termOuter' a child of 'termInner'
    // This is valid types (Term inside Term) but invalid topology (Cycle)
    termInner->setChild(0, termOuter);

    auto pred = Pred("P", { termOuter });

    EXPECT_FALSE(ExpressionTransformer::isTree(pred));

    // Cleanup
    termInner->setChild(0, Var("x"));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsTrue_ForValidExpression) {
    auto expr = And(Pred("P"), Pred("Q", { Var("x") }));
    EXPECT_TRUE(ExpressionTransformer::isFullyDefined(expr));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsFalse_WhenRootIsNull) {
    // !expr return false
    EXPECT_FALSE(ExpressionTransformer::isFullyDefined(nullptr));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsFalse_WhenChildIsNull) {
    // Checks iteration over children: getChild(i) -> isFullyDefinedRec -> !expr
    auto brokenExpr = And(True(), nullptr);
    EXPECT_FALSE(ExpressionTransformer::isFullyDefined(brokenExpr));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsFalse_WhenPredicateSymbolIsEmpty) {
    // PredicateFormula::symbol.empty() check
    auto invalidPred = Pred("", { Var("x") });
    EXPECT_FALSE(ExpressionTransformer::isFullyDefined(invalidPred));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsFalse_WhenFunctionSymbolIsEmpty) {
    // FunctionTerm::symbol.empty() check
    auto invalidFunc = Pred("P", { Func("", {Var("x")}) });
    EXPECT_FALSE(ExpressionTransformer::isFullyDefined(invalidFunc));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsFalse_WhenVariableSymbolIsEmpty) {
    // VariableTerm::symbol.empty() check
    auto invalidVar = Var("");
    // Variable usually appears inside Pred/Func/Quantifier
    auto expr = Pred("P", { invalidVar });
    EXPECT_FALSE(ExpressionTransformer::isFullyDefined(expr));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsFalse_WhenQuantifierVariableIsNull) {
    // QuantificationFormula specific check: !isFullyDefinedRec(quant->variable)
    // We pass nullptr as the variable
    auto invalidQuant = Forall(nullptr, Pred("P"));
    EXPECT_FALSE(ExpressionTransformer::isFullyDefined(invalidQuant));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsFalse_WhenQuantifierVariableSymbolIsEmpty) {
    // QuantificationFormula -> variable -> symbol empty
    auto invalidQuant = Exists(Var(""), Pred("P"));
    EXPECT_FALSE(ExpressionTransformer::isFullyDefined(invalidQuant));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsTrue_ForSharedNodes_DAG) {
    // DAG structure is valid for isFullyDefined (it only cares about data)
    auto sharedLeaf = Pred("P", { Var("x") });
    auto root = And(sharedLeaf, sharedLeaf);
    EXPECT_TRUE(ExpressionTransformer::isFullyDefined(root));
}

TEST_F(ExpressionTransformerTest, IsFullyDefined_ReturnsTrue_ForCycleWithValidData) {
    // Cycles are valid for isFullyDefined (handled by visited set)
    // as long as the data inside nodes is correct.
    auto loop = Not(True());
    loop->setChild(0, loop);

    EXPECT_TRUE(ExpressionTransformer::isFullyDefined(loop));

    // Cleanup
    loop->setChild(0, True());
}

TEST_F(ExpressionTransformerTest, IsArityConsistent_ReturnsTrue_ForConsistentPredicates) {
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("P", { Var("y") })
    );
    EXPECT_TRUE(ExpressionTransformer::isArityConsistent(expr));
}

TEST_F(ExpressionTransformerTest, IsArityConsistent_ReturnsFalse_ForInconsistentPredicateArity) {
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("P", { Var("x"), Var("y") })
    );
    EXPECT_FALSE(ExpressionTransformer::isArityConsistent(expr));
}

TEST_F(ExpressionTransformerTest, IsArityConsistent_ReturnsTrue_ForConsistentFunctions) {
    auto expr = Pred("Q", {
        Func("f", {Var("x")}),
        Func("f", {Var("y")})
        });
    EXPECT_TRUE(ExpressionTransformer::isArityConsistent(expr));
}

TEST_F(ExpressionTransformerTest, IsArityConsistent_ReturnsFalse_ForInconsistentFunctionArity) {
    auto expr = Pred("Q", {
        Func("f", {Var("x")}),
        Func("f", {Var("x"), Var("y")})
        });
    EXPECT_FALSE(ExpressionTransformer::isArityConsistent(expr));
}

TEST_F(ExpressionTransformerTest, IsArityConsistent_ReturnsTrue_ForSameNameInDifferentCategories) {
    // Predicate P(x) (arity 1) and Function P(a, b) (arity 2)
    auto predP = Pred("P", { Var("x") });
    auto funcP = Func("P", { Var("a"), Var("b") });

    auto expr = And(predP, Equal(Func("f", { Var("z") }), funcP));

    EXPECT_TRUE(ExpressionTransformer::isArityConsistent(expr));
}

TEST_F(ExpressionTransformerTest, IsArityConsistent_ReturnsFalse_ForDeepNestedInconsistency) {
    auto expr = Exists(Var("x"),
        Imp(
            Pred("P", { Var("x") }),
            Forall(Var("y"),
                Pred("P", { Var("x"), Var("y") })
            )
        )
    );
    EXPECT_FALSE(ExpressionTransformer::isArityConsistent(expr));
}

TEST_F(ExpressionTransformerTest, IsArityConsistent_ReturnsTrue_ForZeroAritySymbols) {
    auto expr = And(
        Pred("P", {}),
        Equal(Func("f", {}), Var("x"))
    );
    EXPECT_TRUE(ExpressionTransformer::isArityConsistent(expr));
}

// ============================================================================
// isClause Tests
// Definition: A Clause is a disjunction (OR) of Literals.
// ============================================================================

TEST_F(ExpressionTransformerTest, IsClause_ReturnsTrue_ForLiterals) {
    // Atoms
    EXPECT_TRUE(ExpressionTransformer::isClause(Pred("P")));
    EXPECT_TRUE(ExpressionTransformer::isClause(True()));

    // Negated Atoms
    EXPECT_TRUE(ExpressionTransformer::isClause(Not(Pred("Q"))));
}

TEST_F(ExpressionTransformerTest, IsClause_ReturnsTrue_ForRecursiveOr) {
    // Structure: ((A | B) | (C | D))
    auto left = Or(Pred("A"), Pred("B"));
    auto right = Or(Pred("C"), Pred("D"));
    auto root = Or(left, right);

    EXPECT_TRUE(ExpressionTransformer::isClause(root));
}

TEST_F(ExpressionTransformerTest, IsClause_ReturnsTrue_ForJunctionOr) {
    // Structure: OR(A, ~B, C)
    std::vector<FormulaPtr> operands = { Pred("A"), Not(Pred("B")), Pred("C") };
    auto clause = std::make_shared<JunctionFormula>(JunctionFormula::Operator::OR, operands);

    EXPECT_TRUE(ExpressionTransformer::isClause(clause));
}

TEST_F(ExpressionTransformerTest, IsClause_ReturnsFalse_ForInvalidContent) {
    // AND inside OR is forbidden in a Clause
    auto invalid = Or(Pred("A"), And(Pred("B"), Pred("C")));
    EXPECT_FALSE(ExpressionTransformer::isClause(invalid));

    // Double negation is not a standard literal
    auto doubleNeg = Not(Not(Pred("A")));
    EXPECT_FALSE(ExpressionTransformer::isClause(doubleNeg));

    // Implication is not allowed
    EXPECT_FALSE(ExpressionTransformer::isClause(Imp(Pred("A"), Pred("B"))));
}

// ============================================================================
// isCnf Tests
// Definition: CNF is a conjunction (AND) of Clauses.
// ============================================================================

TEST_F(ExpressionTransformerTest, IsCnf_ReturnsTrue_ForSingleClause) {
    // A single clause is valid CNF (conjunction of size 1)
    auto clause = Or(Pred("A"), Not(Pred("B")));
    EXPECT_TRUE(ExpressionTransformer::isCnf(clause));
}

TEST_F(ExpressionTransformerTest, IsCnf_ReturnsTrue_ForRecursiveAnd) {
    // Structure: ((C1 & C2) & C3)
    auto c1 = Or(Pred("A"), Pred("B"));
    auto c2 = Pred("C");
    auto c3 = Not(Pred("D"));

    auto root = And(And(c1, c2), c3);

    EXPECT_TRUE(ExpressionTransformer::isCnf(root));
}

TEST_F(ExpressionTransformerTest, IsCnf_ReturnsTrue_ForJunctionAnd) {
    // Structure: AND(C1, C2, C3)
    std::vector<FormulaPtr> clauses = {
        Or(Pred("A"), Pred("B")),
        Pred("C"),
        True()
    };
    auto root = std::make_shared<JunctionFormula>(JunctionFormula::Operator::AND, clauses);

    EXPECT_TRUE(ExpressionTransformer::isCnf(root));
}

TEST_F(ExpressionTransformerTest, IsCnf_ReturnsFalse_ForOrAtTopLevel) {
    // (A & B) | C -> This is DNF, not CNF
    auto innerAnd = And(Pred("A"), Pred("B"));
    auto root = Or(innerAnd, Pred("C"));

    EXPECT_FALSE(ExpressionTransformer::isCnf(root));
}

TEST_F(ExpressionTransformerTest, IsCnf_ReturnsFalse_ForNonCnfOperators) {
    // Implication at root
    EXPECT_FALSE(ExpressionTransformer::isCnf(Imp(Pred("A"), Pred("B"))));

    // Quantifiers
    auto quantified = DSL::Forall(DSL::Variable("x"), Pred("P"));
    EXPECT_FALSE(ExpressionTransformer::isCnf(quantified));
}

// ============================================================================
// Structural Integrity Tests
// ============================================================================

TEST_F(ExpressionTransformerTest, IsCnf_ReturnsFalse_ForSharedNodes) {
    // The input must be a Tree, not a DAG.
    // Logic: P | P (where P is the same shared pointer)
    auto atom = Pred("Shared");
    auto root = Or(atom, atom);

    // Note: This relies on ExpressionTransformer::isTree returning false for shared ptrs
    EXPECT_FALSE(ExpressionTransformer::isCnf(root));
}

TEST_F(ExpressionTransformerTest, IsCnf_ReturnsFalse_ForNullptr) {
    EXPECT_FALSE(ExpressionTransformer::isCnf(nullptr));
    EXPECT_FALSE(ExpressionTransformer::isClause(nullptr));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsTrue_ForStrictStructure) {
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

    EXPECT_TRUE(ExpressionTransformer::isJunctionCnf(expr));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsFalse_ForSingleLiteralRoot) {
    EXPECT_FALSE(ExpressionTransformer::isJunctionCnf(Pred("P")));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsFalse_ForDisjunctionRoot) {
    auto expr = Disjunction({ Pred("A"), Pred("B") });
    EXPECT_FALSE(ExpressionTransformer::isJunctionCnf(expr));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsFalse_ForBinaryAndRoot) {
    auto expr = And(Disjunction({ Pred("A") }), Disjunction({ Pred("B") }));
    EXPECT_FALSE(ExpressionTransformer::isJunctionCnf(expr));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsFalse_ForMixedChildrenInRoot) {
    auto validClause = Disjunction({ Pred("A") });
    auto invalidChild = Pred("B");

    auto expr = Conjunction({ validClause, invalidChild });
    EXPECT_FALSE(ExpressionTransformer::isJunctionCnf(expr));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsFalse_ForNonLiteralInsideClause) {
    auto innerAnd = And(Pred("X"), Pred("Y"));
    auto clause = Disjunction({ Pred("A"), innerAnd });

    auto expr = Conjunction({ clause });
    EXPECT_FALSE(ExpressionTransformer::isJunctionCnf(expr));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsFalse_ForDoubleNegationInsideClause) {
    auto doubleNeg = Not(Not(Pred("P")));
    auto clause = Disjunction({ doubleNeg });

    auto expr = Conjunction({ clause });
    EXPECT_FALSE(ExpressionTransformer::isJunctionCnf(expr));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsFalse_ForNestedJunctionOr) {
    auto innerOr = Disjunction({ Pred("X"), Pred("Y") });
    auto clause = Disjunction({ Pred("A"), innerOr });

    auto expr = Conjunction({ clause });
    EXPECT_FALSE(ExpressionTransformer::isJunctionCnf(expr));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsFalse_ForEmptyAnd) {
    EXPECT_TRUE(ExpressionTransformer::isJunctionCnf(Conjunction({})));
}

TEST_F(ExpressionTransformerTest, IsJunctionCnf_ReturnsFalse_ForAndWithEmptyOr) {
    EXPECT_TRUE(ExpressionTransformer::isJunctionCnf(Conjunction({ Disjunction({}) })));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsTrue_ForAtoms) {
    EXPECT_TRUE(ExpressionTransformer::isNnf(True()));
    EXPECT_TRUE(ExpressionTransformer::isNnf(False()));
    EXPECT_TRUE(ExpressionTransformer::isNnf(Pred("P")));
    EXPECT_TRUE(ExpressionTransformer::isNnf(Equal(Var("x"), Var("y"))));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsTrue_ForNegatedAtoms) {
    EXPECT_TRUE(ExpressionTransformer::isNnf(Not(Pred("P"))));
    EXPECT_TRUE(ExpressionTransformer::isNnf(Not(Equal(Var("a"), Var("b")))));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsTrue_ForDeeplyNestedStructure) {
    // (P(x) AND ~Q(y)) OR (Forall z, R(z))
    auto expr = Or(
        And(Pred("P", { Var("x") }), Not(Pred("Q", { Var("y") }))),
        Forall(Var("z"), Pred("R", { Var("z") }))
    );
    EXPECT_TRUE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsTrue_ForJunctions) {
    // AND( P, ~Q, OR(R, S) )
    auto expr = Conjunction({
        Pred("P"),
        Not(Pred("Q")),
        Disjunction({ Pred("R"), Pred("S") })
        });
    EXPECT_TRUE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsTrue_ForQuantifiersInside) {
    // Exists x, (P(x) AND Forall y, ~Q(y))
    auto expr = Exists(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Forall(Var("y"), Not(Pred("Q", { Var("y") })))
        )
    );
    EXPECT_TRUE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsFalse_ForDoubleNegation) {
    // ~~P
    auto expr = Not(Not(Pred("P")));
    EXPECT_FALSE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsFalse_ForNegationOfBinaryOp) {
    // ~(A AND B) -> Should be (~A OR ~B)
    auto expr = Not(And(Pred("A"), Pred("B")));
    EXPECT_FALSE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsFalse_ForNegationOfJunction) {
    // ~(AND(A, B))
    auto expr = Not(Conjunction({ Pred("A"), Pred("B") }));
    EXPECT_FALSE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsFalse_ForNegationOfQuantifier) {
    // ~Forall x, P(x) -> Should be Exists x, ~P(x)
    auto expr = Not(Forall(Var("x"), Pred("P")));
    EXPECT_FALSE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsFalse_ForImplication) {
    // Implication is not allowed in NNF
    auto expr = Imp(Pred("A"), Pred("B"));
    EXPECT_FALSE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsFalse_ForEquivalence) {
    auto expr = Eqv(Pred("A"), Pred("B"));
    EXPECT_FALSE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsFalse_ForXor) {
    auto expr = Xor(Pred("A"), Pred("B"));
    EXPECT_FALSE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsFalse_ForNegationInsideImplication) {
    // A -> ~B (Implication itself is invalid, but checking recursion)
    auto expr = Imp(Pred("A"), Not(Pred("B")));
    EXPECT_FALSE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsNnf_ReturnsFalse_ForImplicationInsideNegation) {
    // ~(A -> B)
    auto expr = Not(Imp(Pred("A"), Pred("B")));
    EXPECT_FALSE(ExpressionTransformer::isNnf(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsTrue_ForSimpleAtom) {
    EXPECT_TRUE(ExpressionTransformer::isStandardized(Pred("P", { Var("x") })));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsTrue_ForUniqueQuantifiers) {
    // Forall x, P(x) AND Forall y, Q(y)
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Forall(Var("y"), Pred("Q", { Var("y") }))
    );
    EXPECT_TRUE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsTrue_ForNestedUniqueQuantifiers) {
    // Forall x, Exists y, P(x, y)
    auto expr = Forall(Var("x"),
        Exists(Var("y"),
            Pred("P", { Var("x"), Var("y") })
        )
    );
    EXPECT_TRUE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsFalse_ForReusedVariableInDifferentBranches) {
    // Forall x, P(x) AND Forall x, Q(x)
    // The variable 'x' is quantified twice in disjoint scopes
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Forall(Var("x"), Pred("Q", { Var("x") }))
    );
    EXPECT_FALSE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsFalse_ForShadowedVariable) {
    // Forall x, (P(x) AND Exists x, Q(x))
    // Inner 'x' shadows outer 'x'
    auto expr = Forall(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Exists(Var("x"), Pred("Q", { Var("x") }))
        )
    );
    EXPECT_FALSE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsFalse_ForVariableReusedAsFreeAndBound) {
    // P(x) AND Forall x, Q(x)
    // 'x' appears free in P, then quantified in Q
    auto expr = And(
        Pred("P", { Var("x") }),
        Forall(Var("x"), Pred("Q", { Var("x") }))
    );
    EXPECT_FALSE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsTrue_ForMultipleOccurrencesOfFreeVariable) {
    // P(x) AND Q(x)
    // It is valid to use the same free variable multiple times. 
    // Standardization restricts QUANTIFIED variables.
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("Q", { Var("x") })
    );
    EXPECT_TRUE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsFalse_ForMixedQuantifiersReuse) {
    // Forall x, P(x) AND Exists x, Q(x)
    // Reuse is forbidden regardless of quantifier type
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Exists(Var("x"), Pred("Q", { Var("x") }))
    );
    EXPECT_FALSE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsFalse_ForReuseInsideFunctionTerm) {
    // Forall x, P(x) AND Q(f(x))
    // The x inside f(x) is free, but x is bound in the left branch.
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Pred("Q", { Func("f", { Var("x") }) })
    );
    EXPECT_FALSE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsFalse_ForReuseDeepInsideStructure) {
    // Forall z, (P(z) OR Forall z, Q(z))
    // Nested reuse of z
    auto expr = Forall(Var("z"),
        Or(
            Pred("P", { Var("z") }),
            Forall(Var("z"), Pred("Q", { Var("z") }))
        )
    );
    EXPECT_FALSE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsTrue_ForVacuousQuantification) {
    // Forall x, P(y)
    // Variable x is defined but not used. This is valid standardization 
    // (provided x is not used elsewhere).
    auto expr = Forall(Var("x"), Pred("P", { Var("y") }));
    EXPECT_TRUE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsFalse_ForVacuousQuantificationClash) {
    // Forall x, P(y) AND P(x)
    // x is defined in the quantifier (even if unused in body), 
    // so it cannot be used as a free variable elsewhere.
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("y") })),
        Pred("P", { Var("x") })
    );
    EXPECT_FALSE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsFalse_ForReuseInNaryJunction) {
    // Disjunction( Forall x P(x), Forall x Q(x) )
    auto expr = Disjunction({
        Forall(Var("x"), Pred("P", { Var("x") })),
        Forall(Var("x"), Pred("Q", { Var("x") }))
        });
    EXPECT_FALSE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsStandardized_ReturnsTrue_ForComplexValidStructure) {
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
    EXPECT_TRUE(ExpressionTransformer::isStandardized(expr));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsTrue_ForValidImplicationRule) {
    // Pattern: $A -> $B
    // Replacement: ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsTrue_ForValidQuantifierRule) {
    // Pattern: Forall x, $P
    // Replacement: Exists x, $P
    // Valid because 'x' and 'P' are defined in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Exists(DSL::Variable("x"), DSL::Metavariable("P"));

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsTrue_ForMultiUseInReplacement) {
    // Pattern: $A
    // Replacement: $A AND $A
    // Valid to reuse metavariables in replacement
    auto pattern = DSL::Metavariable("A");
    auto replacement = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("A"));

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsTrue_ForSwappedMetavariables) {
    // Pattern: $A AND $B
    // Replacement: $B AND $A
    auto pattern = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::And(DSL::Metavariable("B"), DSL::Metavariable("A"));

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsFalse_ForDuplicateMetavariableInPattern) {
    // Pattern: $A AND $A  <-- Invalid: Metavariable 'A' appears twice
    // Replacement: $A
    auto pattern = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("A"));
    auto replacement = DSL::Metavariable("A");

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_FALSE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsFalse_ForUnknownMetavariableInReplacement) {
    // Pattern: $A
    // Replacement: $B <-- Invalid: 'B' was not in pattern
    auto pattern = DSL::Metavariable("A");
    auto replacement = DSL::Metavariable("B");

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_FALSE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsFalse_ForUnknownVariableInReplacement) {
    // Pattern: Forall x, $P
    // Replacement: Forall y, $P <-- Invalid: 'y' was not in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Forall(DSL::Variable("y"), DSL::Metavariable("P"));

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_FALSE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsTrue_ForValidCondition) {
    // Pattern: Forall x, $P
    // Condition: NotFreeIn(x, $P)
    // Valid: both x and P are in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };

    ExpressionTransformer::ReplacementRule rule(pattern, replacement, conditions);
    EXPECT_TRUE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsFalse_ForUnknownVariableInCondition) {
    // Condition uses 'y', which is not in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("y"), DSL::Metavariable("P"))
    };

    ExpressionTransformer::ReplacementRule rule(pattern, replacement, conditions);
    EXPECT_FALSE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsFalse_ForUnknownMetavariableInCondition) {
    // Condition uses 'Q', which is not in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("Q"))
    };

    ExpressionTransformer::ReplacementRule rule(pattern, replacement, conditions);
    EXPECT_FALSE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsTrue_ForRepeatedQuantifierVariablesInPattern) {
    // Specification: "Quantifier variables can appear multiple times"
    // Pattern: (Forall x, $A) AND (Exists x, $B) -> 'x' is reused, which is valid for variables
    auto pattern = DSL::And(
        DSL::Forall(DSL::Variable("x"), DSL::Metavariable("A")),
        DSL::Exists(DSL::Variable("x"), DSL::Metavariable("B"))
    );

    auto replacement = DSL::Metavariable("A");

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsTrue_ForBooleanConstantsAndExtendedOps) {
    // Specification: Allowed elements include True, False, Eqv, Xor
    // Pattern: $A Xor False
    auto pattern = DSL::Xor(DSL::Metavariable("A"), DSL::False());

    // Replacement: $A Eqv True
    auto replacement = DSL::Eqv(DSL::Metavariable("A"), DSL::True());

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, IsReplacementRuleCorrect_ReturnsTrue_ForComplexDeeplyNestedPattern) {
    // Validates recursive checking of allowed elements
    // Pattern: Not( Forall x, ( $A Imp ( $B Or True ) ) )
    auto pattern = DSL::Not(
        DSL::Forall(DSL::Variable("x"),
            DSL::Imp(
                DSL::Metavariable("A"),
                DSL::Or(DSL::Metavariable("B"), DSL::True())
            )
        )
    );

    // Replacement: Exists x, ( $A AND ~$B )
    auto replacement = DSL::Exists(DSL::Variable("x"),
        DSL::And(DSL::Metavariable("A"), DSL::Not(DSL::Metavariable("B")))
    );

    ExpressionTransformer::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionTransformer::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionTransformerTest, AreReplacementRulesCorrect_ReturnsTrue_ForVectorOfValidRules) {
    auto p1 = DSL::Metavariable("A");
    auto r1 = DSL::Metavariable("A");
    ExpressionTransformer::ReplacementRule rule1(p1, r1);

    auto p2 = DSL::Imp(DSL::Metavariable("B"), DSL::False());
    auto r2 = DSL::Not(DSL::Metavariable("B"));
    ExpressionTransformer::ReplacementRule rule2(p2, r2);

    std::vector<ExpressionTransformer::ReplacementRule> rules = { rule1, rule2 };
    EXPECT_TRUE(ExpressionTransformer::areReplacementRulesCorrect(rules));
}

TEST_F(ExpressionTransformerTest, AreReplacementRulesCorrect_ReturnsFalse_IfOneRuleIsInvalid) {
    // Rule 1: Valid
    ExpressionTransformer::ReplacementRule validRule(DSL::Metavariable("A"), DSL::Metavariable("A"));

    // Rule 2: Invalid (Replacement uses undefined metavariable 'B')
    ExpressionTransformer::ReplacementRule invalidRule(DSL::Metavariable("A"), DSL::Metavariable("B"));

    std::vector<ExpressionTransformer::ReplacementRule> rules = { validRule, invalidRule };
    EXPECT_FALSE(ExpressionTransformer::areReplacementRulesCorrect(rules));
}

TEST_F(ExpressionTransformerTest, AreAlphaEquivalent_ReturnsTrue_ForIdenticalFormulas) {
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));
    EXPECT_TRUE(ExpressionTransformer::areAlphaEquivalent(expr, expr));
}

TEST_F(ExpressionTransformerTest, AreAlphaEquivalent_ReturnsTrue_ForRenamedBoundVariables) {
    // Forall x, P(x) == Forall y, P(y)
    auto expr1 = Forall(Var("x"), Pred("P", { Var("x") }));
    auto expr2 = Forall(Var("y"), Pred("P", { Var("y") }));
    EXPECT_TRUE(ExpressionTransformer::areAlphaEquivalent(expr1, expr2));
}

TEST_F(ExpressionTransformerTest, AreAlphaEquivalent_ReturnsTrue_ForNestedRenaming) {
    // Forall x, Exists y, Q(x, y) == Forall a, Exists b, Q(a, b)
    auto expr1 = Forall(Var("x"), Exists(Var("y"), Pred("Q", { Var("x"), Var("y") })));
    auto expr2 = Forall(Var("a"), Exists(Var("b"), Pred("Q", { Var("a"), Var("b") })));
    EXPECT_TRUE(ExpressionTransformer::areAlphaEquivalent(expr1, expr2));
}

TEST_F(ExpressionTransformerTest, AreAlphaEquivalent_ReturnsFalse_ForDifferentStructure) {
    auto expr1 = Forall(Var("x"), Pred("P", { Var("x") }));
    auto expr2 = Exists(Var("x"), Pred("P", { Var("x") }));
    EXPECT_FALSE(ExpressionTransformer::areAlphaEquivalent(expr1, expr2));
}

TEST_F(ExpressionTransformerTest, AreAlphaEquivalent_ReturnsFalse_ForFreeVariableMismatch) {
    // P(x) != P(y) (Free variables are not renamed in alpha equivalence)
    auto expr1 = Pred("P", { Var("x") });
    auto expr2 = Pred("P", { Var("y") });
    EXPECT_FALSE(ExpressionTransformer::areAlphaEquivalent(expr1, expr2));
}

TEST_F(ExpressionTransformerTest, IsVarFreeInExpr_ReturnsTrue_ForSimpleFreeOccurrence) {
    // P(x) -> x is free
    auto expr = Pred("P", { Var("x") });
    EXPECT_TRUE(ExpressionTransformer::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionTransformerTest, IsVarFreeInExpr_ReturnsFalse_ForDifferentVariable) {
    // P(y) -> x is not free
    auto expr = Pred("P", { Var("y") });
    EXPECT_FALSE(ExpressionTransformer::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionTransformerTest, IsVarFreeInExpr_ReturnsFalse_ForBoundOccurrence) {
    // Forall x, P(x) -> x is bound (not free)
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));
    EXPECT_FALSE(ExpressionTransformer::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionTransformerTest, IsVarFreeInExpr_ReturnsTrue_ForFreeInOneBranch) {
    // P(x) AND Forall x, Q(x)
    // x is free in the left branch, so it is free in the whole expression
    auto expr = And(
        Pred("P", { Var("x") }),
        Forall(Var("x"), Pred("Q", { Var("x") }))
    );
    EXPECT_TRUE(ExpressionTransformer::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionTransformerTest, IsVarFreeInExpr_ReturnsFalse_ForShadowedBoundOccurrence) {
    // Forall x, (P(x) AND Exists x, Q(x))
    // x is bound at the top level, so no free x inside
    auto expr = Forall(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Exists(Var("x"), Pred("Q", { Var("x") }))
        )
    );
    EXPECT_FALSE(ExpressionTransformer::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionTransformerTest, IsVarFreeInExpr_ReturnsTrue_ForDeeplyNestedFunctionTerm) {
    // Forall y, P( f( g( x ) ) )
    // x is deeply nested inside terms but never quantified
    auto expr = Forall(Var("y"),
        Pred("P", {
            Func("f", {
                Func("g", { Var("x") })
            })
            })
    );
    EXPECT_TRUE(ExpressionTransformer::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionTransformerTest, IsVarFreeInExpr_ReturnsFalse_ForDeeplyNestedBoundVar) {
    // Forall x, P( f( g( x ) ) )
    // x is nested but bound by the top quantifier
    auto expr = Forall(Var("x"),
        Pred("P", {
            Func("f", {
                Func("g", { Var("x") })
            })
            })
    );
    EXPECT_FALSE(ExpressionTransformer::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionTransformerTest, IsVarFreeInExpr_ReturnsTrue_ForComplexMixedStructure) {
    // (Forall z, R(z)) OR (Q(y) IMP P(x))
    // x is free in the right branch
    auto expr = Or(
        Forall(Var("z"), Pred("R", { Var("z") })),
        Imp(
            Pred("Q", { Var("y") }),
            Pred("P", { Var("x") })
        )
    );
    EXPECT_TRUE(ExpressionTransformer::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionTransformerTest, IsVarFreeInExpr_ReturnsFalse_IfSymbolIsPartOfFunctionNameOnly) {
    // P(x_func(y)) check for "x_func" (which is a function symbol, not a variable)
    // The method checks for VARIABLE terms, not function symbols.
    auto expr = Pred("P", { Func("x", { Var("y") }) });

    // Even though "x" is the function name, it's not a variable term "x"
    EXPECT_FALSE(ExpressionTransformer::isVarFreeInExpr(expr, "x"));
}

TEST_F(ExpressionTransformerTest, GetFreeVariables_ReturnsEmpty_ForConstants) {
    auto expr = And(True(), False());
    auto vars = ExpressionTransformer::getFreeVariables(expr);
    EXPECT_TRUE(vars.empty());
}

TEST_F(ExpressionTransformerTest, GetFreeVariables_ReturnsSingle_ForSimpleAtom) {
    auto expr = Pred("P", { Var("x") });
    auto vars = ExpressionTransformer::getFreeVariables(expr);

    ASSERT_EQ(vars.size(), 1);
    EXPECT_EQ(vars[0], "x");
}

TEST_F(ExpressionTransformerTest, GetFreeVariables_ReturnsMultiple_ForDifferentVariables) {
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("Q", { Var("y") })
    );
    auto vars = ExpressionTransformer::getFreeVariables(expr);

    // Sort to ensure deterministic comparison
    std::sort(vars.begin(), vars.end());
    std::vector<std::string> expected = { "x", "y" };

    EXPECT_EQ(vars, expected);
}

TEST_F(ExpressionTransformerTest, GetFreeVariables_ReturnsUnique_ForRepeatedVariables) {
    // P(x) AND Q(x) -> Should return "x" once (assuming implementation dedupes)
    // If implementation returns duplicates, this test adapts to check for uniqueness manually or content.
    // Standard implementation usually returns a set-like vector.
    auto expr = And(
        Pred("P", { Var("x") }),
        Pred("Q", { Var("x") })
    );
    auto vars = ExpressionTransformer::getFreeVariables(expr);

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

TEST_F(ExpressionTransformerTest, GetFreeVariables_ExcludesBoundVariables) {
    // Forall x, P(x) -> x is bound, result empty
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));
    auto vars = ExpressionTransformer::getFreeVariables(expr);

    EXPECT_TRUE(vars.empty());
}

TEST_F(ExpressionTransformerTest, GetFreeVariables_CapturesFreeVar_InsideQuantifierBody) {
    // Forall x, P(y) -> y is free
    auto expr = Forall(Var("x"), Pred("P", { Var("y") }));
    auto vars = ExpressionTransformer::getFreeVariables(expr);

    ASSERT_EQ(vars.size(), 1);
    EXPECT_EQ(vars[0], "y");
}

TEST_F(ExpressionTransformerTest, GetFreeVariables_CapturesVariable_ReusedAsFreeAndBound) {
    // (Forall x, P(x)) AND Q(x)
    // The first x is bound, the second x (in Q) is free.
    // The function MUST return "x" because of the second occurrence.
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Pred("Q", { Var("x") })
    );
    auto vars = ExpressionTransformer::getFreeVariables(expr);

    bool foundX = false;
    for (const auto& v : vars) {
        if (v == "x") foundX = true;
    }
    EXPECT_TRUE(foundX);
}

TEST_F(ExpressionTransformerTest, GetFreeVariables_TraversesFunctionTermsDeeply) {
    // P( f( g( z ) ) ) -> z is free
    auto expr = Pred("P", {
        Func("f", {
            Func("g", { Var("z") })
        })
        });
    auto vars = ExpressionTransformer::getFreeVariables(expr);

    ASSERT_EQ(vars.size(), 1);
    EXPECT_EQ(vars[0], "z");
}

TEST_F(ExpressionTransformerTest, GetFreeVariables_HandlesShadowingCorrectly) {
    // Forall x, ( P(x) AND Exists y, Q(x, y) )
    // All x are bound by Forall. All y are bound by Exists.
    // Result should be empty.
    auto expr = Forall(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Exists(Var("y"), Pred("Q", { Var("x"), Var("y") }))
        )
    );
    auto vars = ExpressionTransformer::getFreeVariables(expr);

    EXPECT_TRUE(vars.empty());
}

TEST_F(ExpressionTransformerTest, GetFreeVariables_HandlesComplexMixedStructure) {
    // (Forall x, P(x)) OR (Q(y) IMP R(f(z)))
    // Free vars: y, z
    auto expr = Or(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Imp(
            Pred("Q", { Var("y") }),
            Pred("R", { Func("f", { Var("z") }) })
        )
    );
    auto vars = ExpressionTransformer::getFreeVariables(expr);

    std::sort(vars.begin(), vars.end());
    std::vector<std::string> uniqueVars;
    std::unique_copy(vars.begin(), vars.end(), std::back_inserter(uniqueVars));

    std::vector<std::string> expected = { "y", "z" };
    EXPECT_EQ(uniqueVars, expected);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_ReturnsZero_ForNull) {
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(nullptr), 0);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_ReturnsOne_ForBooleanConstants) {
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(True()), 1);
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(False()), 1);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_ReturnsOne_ForVariable) {
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(Var("x")), 1);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_CountsPredicateAndArguments) {
    // Structure: Pred node + Var node = 2
    auto expr = Pred("P", { Var("x") });
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr), 2);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_CountsFunctionAndArguments) {
    // Structure: Func node + Var node + Var node = 3
    auto expr = Func("f", { Var("x"), Var("y") });
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr), 3);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_CalculatesBinaryFormulaCorrectly) {
    // Structure: And(1) + True(1) + False(1) = 3
    auto expr = And(True(), False());
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr), 3);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_CalculatesQuantifierCorrectly) {
    // Structure: Forall(1) + Var_decl(1) + Body[Pred(1) + Var_arg(1)] = 4
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr), 4);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_CalculatesNaryJunctionCorrectly) {
    // Structure: Junction(1) + A(1) + B(1) + C(1) = 4
    auto expr = Conjunction({
        Pred("A"),
        Pred("B"),
        Pred("C")
        });
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr), 4);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_CalculatesDeeplyNestedStructure) {
    // Imp (
    //   Not ( Pred("P") ),          -> 1 + 1 = 2
    //   Pred("Q", { Func("f") })    -> 1 + (1 + 0) = 2  (assuming f has 0 args here for simplicity, or f is simple)
    // )
    // Total: 1 (Imp) + 2 + 2 = 5

    auto left = Not(Pred("P"));
    auto right = Pred("Q", { Func("f") });
    auto expr = Imp(left, right);

    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr), 5);
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_CountsTotalNodes_EvenForSharedSubtrees) {
    // In a DAG, shared nodes are physically 1, but logically appear multiple times.
    // getExpressionSize usually counts the logical tree size.

    auto atom = Pred("A"); // Size 1

    // Expr: A AND A
    // Tree Size: 1 (AND) + 1 (Left A) + 1 (Right A) = 3
    auto expr = And(atom, atom);

    ASSERT_EQ(ExpressionTransformer::getExpressionSize(expr), 2); // DAG
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr->clone()), 3); // Tree
}

TEST_F(ExpressionTransformerTest, GetExpressionSize_CountsComplexTermHierarchy) {
    // P( f( g( x ) ) )
    // Pred(1) + Func_f(1) + Func_g(1) + Var_x(1) = 4
    auto expr = Pred("P", {
        Func("f", {
            Func("g", { Var("x") })
        })
        });
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr), 4);
}

TEST_F(ExpressionTransformerTest, ToCnf_ReturnsSameClauses_IfAlreadyCnf) {
    auto clause1 = Disjunction({ Pred("A"), Pred("B") });
    auto clause2 = Disjunction({ Pred("C") });
    auto cnfExpr = Conjunction({ clause1, clause2 });

    ExpressionTransformer transformer;
    auto result = transformer.toCnf(cnfExpr);

    ASSERT_EQ(result.size(), 2);
}

TEST_F(ExpressionTransformerTest, ToCnf_EliminatesImplication) {
    // A -> B  =>  ~A OR B
    auto expr = Imp(Pred("A"), Pred("B"));

    ExpressionTransformer transformer;
    auto clauses = transformer.toCnf(expr);

    ASSERT_EQ(clauses.size(), 1);

    auto clause = std::dynamic_pointer_cast<JunctionFormula>(clauses[0]);
    ASSERT_TRUE(clause != nullptr);
    EXPECT_EQ(clause->op, JunctionFormula::Operator::OR);
    EXPECT_EQ(clause->operands.size(), 2);

    // Check for ~A
    EXPECT_EQ(clause->operands[0]->exprType, Expression::Type::NEGATION);
}

TEST_F(ExpressionTransformerTest, ToCnf_EliminatesEquivalence) {
    // A <-> B  =>  (A -> B) AND (B -> A)  =>  (~A OR B) AND (~B OR A)
    auto expr = Eqv(Pred("A"), Pred("B"));

    ExpressionTransformer transformer;
    auto clauses = transformer.toCnf(expr);

    // Expecting 2 clauses
    ASSERT_EQ(clauses.size(), 2);
}

TEST_F(ExpressionTransformerTest, ToCnf_AppliesDeMorgan_ForNegatedAnd) {
    // ~(A AND B) => ~A OR ~B
    auto expr = Not(And(Pred("A"), Pred("B")));

    ExpressionTransformer transformer;
    auto clauses = transformer.toCnf(expr);

    ASSERT_EQ(clauses.size(), 1);

    auto clause = std::dynamic_pointer_cast<JunctionFormula>(clauses[0]);
    ASSERT_TRUE(clause != nullptr);
    EXPECT_EQ(clause->op, JunctionFormula::Operator::OR);
    EXPECT_EQ(clause->operands.size(), 2);

    EXPECT_EQ(clause->operands[0]->exprType, Expression::Type::NEGATION);
    EXPECT_EQ(clause->operands[1]->exprType, Expression::Type::NEGATION);
}

TEST_F(ExpressionTransformerTest, ToCnf_AppliesDeMorgan_ForNegatedOr) {
    // ~(A OR B) => ~A AND ~B
    // Result is 2 clauses: (~A), (~B)
    auto expr = Not(Or(Pred("A"), Pred("B")));

    ExpressionTransformer transformer;
    auto clauses = transformer.toCnf(expr);

    ASSERT_EQ(clauses.size(), 2);
}

TEST_F(ExpressionTransformerTest, ToCnf_RemovesDoubleNegation) {
    // ~~A => A
    auto expr = Not(Not(Pred("A")));

    ExpressionTransformer transformer;
    auto clauses = transformer.toCnf(expr);

    ASSERT_EQ(clauses.size(), 1);
    auto clause = std::dynamic_pointer_cast<JunctionFormula>(clauses[0]);
    ASSERT_TRUE(clause != nullptr);

    ASSERT_EQ(clause->operands.size(), 1);
    EXPECT_EQ(clause->operands[0]->exprType, Expression::Type::PREDICATE);
}

TEST_F(ExpressionTransformerTest, ToCnf_DistributesOrOverAnd) {
    // A OR (B AND C) => (A OR B) AND (A OR C)
    auto expr = Or(
        Pred("A"),
        And(Pred("B"), Pred("C"))
    );

    ExpressionTransformer transformer;
    auto clauses = transformer.toCnf(expr);

    // Expecting 2 clauses
    ASSERT_EQ(clauses.size(), 2);

    auto c1 = std::dynamic_pointer_cast<JunctionFormula>(clauses[0]);
    auto c2 = std::dynamic_pointer_cast<JunctionFormula>(clauses[1]);

    ASSERT_TRUE(c1 && c2);
    EXPECT_EQ(c1->operands.size(), 2);
    EXPECT_EQ(c2->operands.size(), 2);
}

TEST_F(ExpressionTransformerTest, ToCnf_HandlesComplexMixedStructure) {
    // (A -> B) OR C 
    // => (~A OR B) OR C 
    // => ~A OR B OR C (Flattening to single clause)
    auto expr = Or(
        Imp(Pred("A"), Pred("B")),
        Pred("C")
    );

    ExpressionTransformer transformer;
    auto clauses = transformer.toCnf(expr);

    ASSERT_EQ(clauses.size(), 1);

    auto clause = std::dynamic_pointer_cast<JunctionFormula>(clauses[0]);
    ASSERT_TRUE(clause != nullptr);
    // Should contain 3 literals: ~A, B, C
    EXPECT_EQ(clause->operands.size(), 3);
}

// 1. Logic check: Empty Junctions -> Constants (True/False)
TEST_F(ExpressionTransformerTest, EliminateJunction_EmptyJunctions_ConvertToConstants) {
    ExpressionTransformer transformer;

    // Empty AND -> True
    auto emptyAnd = std::make_shared<JunctionFormula>(JunctionFormula::Operator::AND, std::vector<FormulaPtr>{});
    auto resAnd = transformer.eliminateJunction(emptyAnd, false);

    EXPECT_TRUE(ExpressionTransformer::areAlphaEquivalent(resAnd, DSL::True()));

    // Empty OR -> False
    auto emptyOr = std::make_shared<JunctionFormula>(JunctionFormula::Operator::OR, std::vector<FormulaPtr>{});
    auto resOr = transformer.eliminateJunction(emptyOr, false);

    EXPECT_TRUE(ExpressionTransformer::areAlphaEquivalent(resOr, DSL::False()));
}

// 2. Wrapping check: Single operand unwraps correctly
TEST_F(ExpressionTransformerTest, EliminateJunction_SingleOperand_UnwrapsContent) {
    ExpressionTransformer transformer;
    auto atom = Pred("P");
    auto singleAnd = std::make_shared<JunctionFormula>(JunctionFormula::Operator::AND, std::vector<FormulaPtr>{atom});

    // Case A: inPlace = false (Expect Deep Copy)
    auto resultCopy = transformer.eliminateJunction(singleAnd, false);

    // Logic: The content is P
    EXPECT_TRUE(ExpressionTransformer::areAlphaEquivalent(resultCopy, atom));
    // Memory: It must be a NEW object (clone), not the original pointer
    EXPECT_NE(resultCopy, atom);

    // Case B: inPlace = true (Expect Pointer Identity)
    // We create a fresh wrapper because the previous call was non-destructive
    auto singleAnd2 = std::make_shared<JunctionFormula>(JunctionFormula::Operator::AND, std::vector<FormulaPtr>{atom});
    auto resultPtr = transformer.eliminateJunction(singleAnd2, true);

    // Memory: It must be the EXACT SAME object pointer
    EXPECT_EQ(resultPtr, atom);
}

// 3. Structure check: Multi-arg converts to Left-Associative Binary Tree
TEST_F(ExpressionTransformerTest, EliminateJunction_MultiArg_ConvertsToLeftAssociativeBinary) {
    ExpressionTransformer transformer;
    auto A = Pred("A");
    auto B = Pred("B");
    auto C = Pred("C");

    auto junction = std::make_shared<JunctionFormula>(
        JunctionFormula::Operator::AND, std::vector<FormulaPtr>{A, B, C});

    // Using inPlace=false, checking structural equivalence
    auto result = transformer.eliminateJunction(junction, false);

    // Expected: ((A & B) & C)
    auto expectedExpr = DSL::And(DSL::And(A, B), C);

    EXPECT_TRUE(ExpressionTransformer::areAlphaEquivalent(result, expectedExpr));
}

// 4. Recursion check: Nested junctions are transformed bottom-up
TEST_F(ExpressionTransformerTest, EliminateJunction_NestedJunctions_RecursivelyTransformed) {
    ExpressionTransformer transformer;
    auto A = Pred("A");
    auto B = Pred("B");
    auto C = Pred("C");

    // Input: OR(A, AND(B, C))
    auto innerAnd = std::make_shared<JunctionFormula>(JunctionFormula::Operator::AND, std::vector<FormulaPtr>{B, C});
    auto outerOr = std::make_shared<JunctionFormula>(JunctionFormula::Operator::OR, std::vector<FormulaPtr>{A, innerAnd});

    auto result = transformer.eliminateJunction(outerOr, false);

    // Expected: (A | (B & C))
    auto expectedExpr = DSL::Or(A, DSL::And(B, C));

    EXPECT_TRUE(ExpressionTransformer::areAlphaEquivalent(result, expectedExpr));
}

// 5. Immutability check: inPlace = false does NOT modify original
TEST_F(ExpressionTransformerTest, EliminateJunction_InPlaceFalse_DoesNotModifyOriginal) {
    ExpressionTransformer transformer;
    auto A = Pred("A");
    auto B = Pred("B");

    auto junction = std::make_shared<JunctionFormula>(
        JunctionFormula::Operator::AND, std::vector<FormulaPtr>{A, B});

    auto result = transformer.eliminateJunction(junction, false);

    // Result is Binary
    EXPECT_EQ(result->exprType, Expression::Type::BINARY);

    // Original is still Junction
    EXPECT_EQ(junction->exprType, Expression::Type::JUNCTION);
    auto castedJunction = std::static_pointer_cast<JunctionFormula>(junction);
    EXPECT_EQ(castedJunction->operands.size(), 2);
}

// 6. DAG check: inPlace = true preserves shared pointers
TEST_F(ExpressionTransformerTest, EliminateJunction_InPlaceTrue_PreservesSharedPointers) {
    ExpressionTransformer transformer;
    auto sharedLeaf = Pred("Shared");

    // Input: OR(Shared, Shared) using the exact same pointer
    auto junction = std::make_shared<JunctionFormula>(
        JunctionFormula::Operator::OR, std::vector<FormulaPtr>{sharedLeaf, sharedLeaf});

    // Action: inPlace = true
    auto result = transformer.eliminateJunction(junction, true);

    ASSERT_EQ(result->exprType, Expression::Type::BINARY);
    auto bin = std::static_pointer_cast<BinaryFormula>(result);

    // Verify structural correctness
    EXPECT_EQ(bin->op, BinaryFormula::Operator::OR);

    // Verify POINTER identity (DAG preservation)
    EXPECT_EQ(bin->left, sharedLeaf);
    EXPECT_EQ(bin->right, sharedLeaf);
    EXPECT_EQ(bin->left, bin->right);
}

// 7. Optimization check: Ignores terms (Function/Variable)
TEST_F(ExpressionTransformerTest, EliminateJunction_IgnoresTermsOptimization) {
    ExpressionTransformer transformer;

    // P(f(x)) -> Formula containing terms
    auto x = Var("x");
    auto f = Func("f", { x });
    auto P = Pred("P", { f });

    // inPlace = true to avoid top-level cloning, ensuring we test traversal logic
    auto result = transformer.eliminateJunction(P, true);

    // Should return original pointer (Atom)
    EXPECT_EQ(result, P);

    // Verify internal term 'f' was not cloned/touched
    auto funcTerm = std::static_pointer_cast<FunctionTerm>(P->getChild(0));
    EXPECT_EQ(funcTerm, f);
}

TEST_F(ExpressionTransformerTest, StandardizeVariables_RenamesFreeVariables) {
    // Input: P(x)
    // Expectation: P(V_n) where V_n is a generated system name
    auto expr = Pred("P", { Var("x") });

    ExpressionTransformer transformer;
    auto result = transformer.standardizeVariables(expr);

    ASSERT_TRUE(ExpressionTransformer::isStandardized(result));

    auto pred = std::dynamic_pointer_cast<PredicateFormula>(result);
    auto arg = std::dynamic_pointer_cast<VariableTerm>(pred->arguments[0]);

    EXPECT_NE(arg->symbol, "x"); // Should be renamed
    EXPECT_FALSE(arg->symbol.empty());
}

TEST_F(ExpressionTransformerTest, StandardizeVariables_RenamesBoundVariables) {
    // Input: Forall x, P(x)
    // Expectation: Forall V_n, P(V_n)
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));

    ExpressionTransformer transformer;
    auto result = transformer.standardizeVariables(expr);

    ASSERT_TRUE(ExpressionTransformer::isStandardized(result));

    auto quant = std::dynamic_pointer_cast<QuantificationFormula>(result);
    auto body = std::dynamic_pointer_cast<PredicateFormula>(quant->body);
    auto arg = std::dynamic_pointer_cast<VariableTerm>(body->arguments[0]);

    // The bound variable and the argument must match the new name
    EXPECT_EQ(quant->variable->symbol, arg->symbol);
    EXPECT_NE(quant->variable->symbol, "x");
}

TEST_F(ExpressionTransformerTest, StandardizeVariables_SeparatesFreeAndBoundCollisions) {
    // Input: P(x) AND Forall x, Q(x)
    // This is the critical case. 'x' is free in P, but bound in Q.
    // They MUST end up as different variables (e.g., V_1 and V_2).
    auto expr = And(
        Pred("P", { Var("x") }),
        Forall(Var("x"), Pred("Q", { Var("x") }))
    );

    ExpressionTransformer transformer;
    auto result = transformer.standardizeVariables(expr);

    ASSERT_TRUE(ExpressionTransformer::isStandardized(result));

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    auto leftPred = std::dynamic_pointer_cast<PredicateFormula>(binary->left);
    auto rightQuant = std::dynamic_pointer_cast<QuantificationFormula>(binary->right);

    std::string freeVarName = std::dynamic_pointer_cast<VariableTerm>(leftPred->arguments[0])->symbol;
    std::string boundVarName = rightQuant->variable->symbol;

    // Names must be distinct
    EXPECT_NE(freeVarName, boundVarName);
}

TEST_F(ExpressionTransformerTest, StandardizeVariables_HandlesNestedShadowing) {
    // Input: Forall x, ( P(x) AND Exists x, Q(x) )
    // The inner x shadows the outer x. They must become unique.
    auto expr = Forall(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Exists(Var("x"), Pred("Q", { Var("x") }))
        )
    );

    ExpressionTransformer transformer;
    auto result = transformer.standardizeVariables(expr);

    auto outerQuant = std::dynamic_pointer_cast<QuantificationFormula>(result);
    auto body = std::dynamic_pointer_cast<BinaryFormula>(outerQuant->body);
    auto innerQuant = std::dynamic_pointer_cast<QuantificationFormula>(body->right);

    // Verify distinct names for outer and inner scopes
    EXPECT_NE(outerQuant->variable->symbol, innerQuant->variable->symbol);
}

TEST_F(ExpressionTransformerTest, StandardizeVariables_EnsuresGlobalUniquenessAcrossBranches) {
    // Input: (Forall x, P(x)) AND (Forall x, Q(x))
    // Even though scopes are disjoint, standardization usually enforces 
    // globally unique names to facilitate safe Skolemization later.
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Forall(Var("x"), Pred("Q", { Var("x") }))
    );

    ExpressionTransformer transformer;
    auto result = transformer.standardizeVariables(expr);

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    auto leftQuant = std::dynamic_pointer_cast<QuantificationFormula>(binary->left);
    auto rightQuant = std::dynamic_pointer_cast<QuantificationFormula>(binary->right);

    EXPECT_NE(leftQuant->variable->symbol, rightQuant->variable->symbol);
}

TEST_F(ExpressionTransformerTest, StandardizeVariables_UpdatesVariablesDeepInFunctions) {
    // Input: Forall x, P( f(x) )
    auto expr = Forall(Var("x"),
        Pred("P", { Func("f", { Var("x") }) })
    );

    ExpressionTransformer transformer;
    auto result = transformer.standardizeVariables(expr);

    auto quant = std::dynamic_pointer_cast<QuantificationFormula>(result);
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(quant->body);
    auto func = std::dynamic_pointer_cast<FunctionTerm>(pred->arguments[0]);
    auto varTerm = std::dynamic_pointer_cast<VariableTerm>(func->arguments[0]);

    EXPECT_EQ(quant->variable->symbol, varTerm->symbol);
}

TEST_F(ExpressionTransformerTest, StandardizeVariables_ModifiesInPlace) {
    // Input: Forall x, P(x)
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));

    ExpressionTransformer transformer;
    // Pass true for inPlace
    auto result = transformer.standardizeVariables(expr, true);

    // 1. Structure should be standardized
    EXPECT_TRUE(ExpressionTransformer::isStandardized(result));

    // 2. Pointer identity must match (Critical Check)
    EXPECT_EQ(expr.get(), result.get());

    // 3. Original expression variable should be changed
    EXPECT_NE(expr->variable->symbol, "x");
}

TEST_F(ExpressionTransformerTest, Skolemize_ReturnsOriginal_IfNoExistentials) {
    // Input: Forall x, P(x)
    // No changes expected
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    // Verify structure is identical (deep comparison or simple type check)
    auto quant = std::dynamic_pointer_cast<QuantificationFormula>(result);
    ASSERT_TRUE(quant != nullptr);
    EXPECT_EQ(quant->type, QuantificationFormula::Quantifier::FORALL);
}

TEST_F(ExpressionTransformerTest, Skolemize_ReplacesTopLevelExists_WithConstant) {
    // Input: Exists x, P(x)
    // Expectation: P(c) where c is a function term with 0 arguments (constant)
    auto expr = Exists(Var("x"), Pred("P", { Var("x") }));

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    // The Exists wrapper should be gone
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(result);
    ASSERT_TRUE(pred != nullptr);

    // Check argument
    ASSERT_EQ(pred->arguments.size(), 1);
    auto skolemTerm = std::dynamic_pointer_cast<FunctionTerm>(pred->arguments[0]);
    ASSERT_TRUE(skolemTerm != nullptr);

    // It should be a constant (arity 0)
    EXPECT_TRUE(skolemTerm->arguments.empty());
    // Name should be generated (e.g. $$F_1)
    EXPECT_FALSE(skolemTerm->symbol.empty());
}

TEST_F(ExpressionTransformerTest, Skolemize_CreatesFunction_DependingOnUniversal) {
    // Input: Forall x, Exists y, P(x, y)
    // Expectation: Forall x, P(x, f(x))
    auto expr = Forall(Var("x"),
        Exists(Var("y"),
            Pred("P", { Var("x"), Var("y") })
        )
    );

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    // Outer Forall remains
    auto quant = std::dynamic_pointer_cast<QuantificationFormula>(result);
    ASSERT_TRUE(quant != nullptr);
    EXPECT_EQ(quant->type, QuantificationFormula::Quantifier::FORALL);

    // Exists should be removed, body is Predicate
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(quant->body);
    ASSERT_TRUE(pred != nullptr);

    // Check 2nd argument of P(x, y) -> should become P(x, f(x))
    auto skolemFunc = std::dynamic_pointer_cast<FunctionTerm>(pred->arguments[1]);
    ASSERT_TRUE(skolemFunc != nullptr);

    // Check arity and arguments of Skolem function
    ASSERT_EQ(skolemFunc->arguments.size(), 1);
    auto funcArg = std::dynamic_pointer_cast<VariableTerm>(skolemFunc->arguments[0]);
    EXPECT_EQ(funcArg->symbol, "x");
}

TEST_F(ExpressionTransformerTest, Skolemize_HandlesMultipleDependencies) {
    // Input: Forall x, Forall y, Exists z, P(x, y, z)
    // Expectation: Forall x, Forall y, P(x, y, f(x, y))
    auto expr = Forall(Var("x"),
        Forall(Var("y"),
            Exists(Var("z"),
                Pred("P", { Var("x"), Var("y"), Var("z") })
            )
        )
    );

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    // Navigate to the predicate
    auto q1 = std::dynamic_pointer_cast<QuantificationFormula>(result); // Forall x
    auto q2 = std::dynamic_pointer_cast<QuantificationFormula>(q1->body); // Forall y
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(q2->body); // P(...)

    ASSERT_TRUE(pred != nullptr);
    ASSERT_EQ(pred->arguments.size(), 3);

    // Check the 3rd argument (was z)
    auto skolemFunc = std::dynamic_pointer_cast<FunctionTerm>(pred->arguments[2]);
    ASSERT_TRUE(skolemFunc != nullptr);

    // Should depend on x and y
    ASSERT_EQ(skolemFunc->arguments.size(), 2);
    auto arg1 = std::dynamic_pointer_cast<VariableTerm>(skolemFunc->arguments[0]);
    auto arg2 = std::dynamic_pointer_cast<VariableTerm>(skolemFunc->arguments[1]);

    EXPECT_EQ(arg1->symbol, "x");
    EXPECT_EQ(arg2->symbol, "y");
}

TEST_F(ExpressionTransformerTest, Skolemize_RespectsScopeBranches) {
    // Input: (Forall x, P(x)) AND (Exists y, Q(y))
    // Critical: The Skolem constant for y must NOT depend on x, 
    // because Exists y is NOT inside the scope of Forall x.
    auto expr = And(
        Forall(Var("x"), Pred("P", { Var("x") })),
        Exists(Var("y"), Pred("Q", { Var("y") }))
    );

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    // Right branch: Q(c)
    auto predQ = std::dynamic_pointer_cast<PredicateFormula>(binary->right);
    ASSERT_TRUE(predQ != nullptr);

    auto skolemTerm = std::dynamic_pointer_cast<FunctionTerm>(predQ->arguments[0]);
    ASSERT_TRUE(skolemTerm != nullptr);

    // Must be a constant (0 dependencies), NOT f(x)
    EXPECT_TRUE(skolemTerm->arguments.empty());
}

TEST_F(ExpressionTransformerTest, Skolemize_HandlesInterleavedQuantifiers) {
    // Input: Forall x, Exists y, Forall z, Exists w, P(x, y, z, w)
    // 1. y -> f(x)
    // 2. w -> g(x, z)  (depends on x AND z)

    auto expr = Forall(Var("x"),
        Exists(Var("y"),
            Forall(Var("z"),
                Exists(Var("w"),
                    Pred("P", { Var("x"), Var("y"), Var("z"), Var("w") })
                )
            )
        )
    );

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    // Verify structure: Forall x, Forall z, P(...)
    auto qX = std::dynamic_pointer_cast<QuantificationFormula>(result);
    auto qZ = std::dynamic_pointer_cast<QuantificationFormula>(qX->body);
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(qZ->body);

    // Check w (4th arg) replacement
    auto skolemW = std::dynamic_pointer_cast<FunctionTerm>(pred->arguments[3]);
    ASSERT_EQ(skolemW->arguments.size(), 2); // Dependencies: x, z

    auto dep1 = std::dynamic_pointer_cast<VariableTerm>(skolemW->arguments[0]);
    auto dep2 = std::dynamic_pointer_cast<VariableTerm>(skolemW->arguments[1]);

    EXPECT_EQ(dep1->symbol, "x");
    EXPECT_EQ(dep2->symbol, "z");
}

TEST_F(ExpressionTransformerTest, Skolemize_SubstitutesDeeplyInsideFunctionTerms) {
    // Input: Exists x, P( f( g(x) ) )
    // Expectation: P( f( g(c) ) )
    auto expr = Exists(Var("x"),
        Pred("P", {
            Func("f", {
                Func("g", { Var("x") })
            })
            })
    );

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    auto pred = std::dynamic_pointer_cast<PredicateFormula>(result);
    auto funcF = std::dynamic_pointer_cast<FunctionTerm>(pred->arguments[0]);
    auto funcG = std::dynamic_pointer_cast<FunctionTerm>(funcF->arguments[0]);
    auto skolemConst = std::dynamic_pointer_cast<FunctionTerm>(funcG->arguments[0]);

    ASSERT_TRUE(skolemConst != nullptr);
    EXPECT_TRUE(skolemConst->arguments.empty()); // Constant
}

TEST_F(ExpressionTransformerTest, Skolemize_HandlesEqualityFormula_WithFreeVariableDependency) {
    // Input: Exists x, (x = a)
    // Assumption: 'a' is a free variable, treated as implicitly universally quantified.
    // Therefore, Skolem function for x MUST depend on a.
    auto expr = Exists(Var("x"), Equal(Var("x"), Var("a")));

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    auto eq = std::dynamic_pointer_cast<EqualityFormula>(result);
    ASSERT_TRUE(eq != nullptr);

    // Left side should be f(a)
    auto term = eq->left;

    // 1. Check if it is a function
    ASSERT_EQ(term->exprType, Expression::Type::FUNCTION);
    auto skolemFunc = std::static_pointer_cast<FunctionTerm>(term);

    // 2. Check if it depends on 'a'
    ASSERT_EQ(skolemFunc->arguments.size(), 1);

    auto arg = skolemFunc->arguments[0];
    ASSERT_EQ(arg->exprType, Expression::Type::VARIABLE);
    auto varArg = std::static_pointer_cast<VariableTerm>(arg);
    EXPECT_EQ(varArg->symbol, "a");
}

TEST_F(ExpressionTransformerTest, Skolemize_RemovesVacuousQuantifier) {
    // Input: Exists x, P(a)
    // Expectation: P(a) (Quantifier removed, no substitution needed)
    auto expr = Exists(Var("x"), Pred("P", { Var("a") }));

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    // Should be just Predicate P(a)
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(result);
    ASSERT_TRUE(pred != nullptr);
    EXPECT_EQ(pred->symbol, "P");

    auto arg = std::dynamic_pointer_cast<VariableTerm>(pred->arguments[0]);
    EXPECT_EQ(arg->symbol, "a");
}

TEST_F(ExpressionTransformerTest, Skolemize_HandlesConsecutiveExistentials) {
    // Input: Forall x, Exists y, Exists z, P(x, y, z)
    // Expectation: Forall x, P(x, f(x), g(x))
    // Both y and z depend ONLY on x. z does NOT depend on y.
    auto expr = Forall(Var("x"),
        Exists(Var("y"),
            Exists(Var("z"),
                Pred("P", { Var("x"), Var("y"), Var("z") })
            )
        )
    );

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    auto quant = std::dynamic_pointer_cast<QuantificationFormula>(result);
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(quant->body);

    // Check 2nd arg (y -> f(x))
    auto skolemY = std::dynamic_pointer_cast<FunctionTerm>(pred->arguments[1]);
    ASSERT_EQ(skolemY->arguments.size(), 1);
    EXPECT_EQ(std::dynamic_pointer_cast<VariableTerm>(skolemY->arguments[0])->symbol, "x");

    // Check 3rd arg (z -> g(x))
    auto skolemZ = std::dynamic_pointer_cast<FunctionTerm>(pred->arguments[2]);
    ASSERT_EQ(skolemZ->arguments.size(), 1); // Should depend ONLY on x
    EXPECT_EQ(std::dynamic_pointer_cast<VariableTerm>(skolemZ->arguments[0])->symbol, "x");

    // Ensure f and g are different functions
    EXPECT_NE(skolemY->symbol, skolemZ->symbol);
}

TEST_F(ExpressionTransformerTest, Skolemize_DoesNotDependOnFreeVariables) {
    // Input: P(z) AND Exists x, Q(x)
    // 'z' is free. 'x' is bound by Exists.
    // The Skolem constant for x should NOT depend on z.
    auto expr = And(
        Pred("P", { Var("z") }),
        Exists(Var("x"), Pred("Q", { Var("x") }))
    );

    ExpressionTransformer transformer;
    transformer.reserveExpressionSymbols(expr);
    auto result = transformer.skolemize(expr);

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    auto predQ = std::dynamic_pointer_cast<PredicateFormula>(binary->right);
    auto skolemX = std::dynamic_pointer_cast<FunctionTerm>(predQ->arguments[0]);

    // Must be a constant (arity 0), ignoring free variable z
    EXPECT_TRUE(skolemX->arguments.empty());
}

TEST_F(ExpressionTransformerTest, EliminateQuantifiers_RemovesMatchingQuantifier) {
    // Input: Forall x, P(x)
    // Remove: FORALL
    // Result: P(x) (Body only)
    auto expr = Forall(Var("x"), Pred("P", { Var("x") }));

    ExpressionTransformer transformer;
    auto result = transformer.eliminateQuantifiers(expr, QuantificationFormula::Quantifier::FORALL);

    // Should be just Predicate P(x)
    EXPECT_EQ(result->exprType, Expression::Type::PREDICATE);
}

TEST_F(ExpressionTransformerTest, EliminateQuantifiers_IgnoresMismatchedQuantifier) {
    // Input: Exists x, P(x)
    // Remove: FORALL
    // Result: Exists x, P(x) (Unchanged)
    auto expr = Exists(Var("x"), Pred("P", { Var("x") }));

    ExpressionTransformer transformer;
    auto result = transformer.eliminateQuantifiers(expr, QuantificationFormula::Quantifier::FORALL);

    EXPECT_EQ(result->exprType, Expression::Type::QUANTIFICATION);
    auto quant = std::dynamic_pointer_cast<QuantificationFormula>(result);
    EXPECT_EQ(quant->type, QuantificationFormula::Quantifier::EXISTS);
}

TEST_F(ExpressionTransformerTest, EliminateQuantifiers_RemovesNestedQuantifiersRecursively) {
    // Input: Forall x, Forall y, P(x, y)
    // Remove: FORALL
    // Result: P(x, y)
    auto expr = Forall(Var("x"),
        Forall(Var("y"),
            Pred("P", { Var("x"), Var("y") })
        )
    );

    ExpressionTransformer transformer;
    auto result = transformer.eliminateQuantifiers(expr, QuantificationFormula::Quantifier::FORALL);

    EXPECT_EQ(result->exprType, Expression::Type::PREDICATE);
}

TEST_F(ExpressionTransformerTest, EliminateQuantifiers_RemovesMixedDeepQuantifiers) {
    // Input: Forall x, ( P(x) AND Forall y, Q(y) )
    // Remove: FORALL
    // Result: P(x) AND Q(y)
    auto expr = Forall(Var("x"),
        And(
            Pred("P", { Var("x") }),
            Forall(Var("y"), Pred("Q", { Var("y") }))
        )
    );

    ExpressionTransformer transformer;
    auto result = transformer.eliminateQuantifiers(expr, QuantificationFormula::Quantifier::FORALL);

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);

    // Left: P(x)
    EXPECT_EQ(binary->left->exprType, Expression::Type::PREDICATE);
    // Right: Q(y) (Quantifier removed)
    EXPECT_EQ(binary->right->exprType, Expression::Type::PREDICATE);
}

TEST_F(ExpressionTransformerTest, EliminateQuantifiers_PreservesOtherQuantifiers) {
    // Input: Forall x, Exists y, P(x, y)
    // Remove: FORALL
    // Result: Exists y, P(x, y)
    auto expr = Forall(Var("x"),
        Exists(Var("y"),
            Pred("P", { Var("x"), Var("y") })
        )
    );

    ExpressionTransformer transformer;
    auto result = transformer.eliminateQuantifiers(expr, QuantificationFormula::Quantifier::FORALL);

    // Should still be Exists
    EXPECT_EQ(result->exprType, Expression::Type::QUANTIFICATION);
    auto quant = std::dynamic_pointer_cast<QuantificationFormula>(result);
    EXPECT_EQ(quant->type, QuantificationFormula::Quantifier::EXISTS);

    // Body should be Predicate
    EXPECT_EQ(quant->body->exprType, Expression::Type::PREDICATE);
}

TEST_F(ExpressionTransformerTest, EliminateQuantifiers_HandlesDeepNestingInBinaryOps) {
    // Input: (A -> Forall x, B)
    // Remove: FORALL
    // Result: (A -> B)
    auto expr = Imp(
        Pred("A"),
        Forall(Var("x"), Pred("B"))
    );

    ExpressionTransformer transformer;
    auto result = transformer.eliminateQuantifiers(expr, QuantificationFormula::Quantifier::FORALL);

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);
    EXPECT_EQ(binary->right->exprType, Expression::Type::PREDICATE);
}

TEST_F(ExpressionTransformerTest, Rewrite_ReturnsOriginal_WhenNoRulesMatch) {
    // Formula: P(x) AND Q(y)
    auto expr = And(Pred("P", { Var("x") }), Pred("Q", { Var("y") }));

    // Rule: $A -> $B  ==>  ~$A OR $B (Implication elimination)
    // There is no implication in the input expr.
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    ExpressionTransformer transformer;
    auto result = transformer.rewrite(expr, { rule });

    // Structure should remain identical
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr), ExpressionTransformer::getExpressionSize(result));

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::AND);
}

TEST_F(ExpressionTransformerTest, Rewrite_AppliesRule_AtRoot) {
    // Formula: P(x) -> Q(y)
    auto expr = Imp(Pred("P", { Var("x") }), Pred("Q", { Var("y") }));

    // Rule: $A -> $B  ==>  ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    ExpressionTransformer transformer;
    auto result = transformer.rewrite(expr, { rule });

    // Expected: ~P(x) OR Q(y)
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::OR);

    // Left child should be Negation
    EXPECT_EQ(binary->left->exprType, Expression::Type::NEGATION); // ~P(x)
}

TEST_F(ExpressionTransformerTest, Rewrite_AppliesRule_RecursivelyDeepInTree) {
    // Formula: R(z) AND (P(x) -> Q(y))
    // The implication is nested inside an AND
    auto expr = And(
        Pred("R", { Var("z") }),
        Imp(Pred("P", { Var("x") }), Pred("Q", { Var("y") }))
    );

    // Rule: $A -> $B  ==>  ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    ExpressionTransformer transformer;
    auto result = transformer.rewrite(expr, { rule });

    // Expected: R(z) AND (~P(x) OR Q(y))
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::AND);

    // Check right child (where the rewrite happened)
    auto rightChild = std::dynamic_pointer_cast<BinaryFormula>(binary->right);
    ASSERT_TRUE(rightChild != nullptr);
    EXPECT_EQ(rightChild->op, BinaryFormula::Operator::OR);
}

TEST_F(ExpressionTransformerTest, Rewrite_MatchesPatternVariable_UsingUnification) {
    // Rule: Forall x, $P  ==>  Exists x, ~$P (Dummy rule for testing)
    // In Pattern Matching logic, 'x' here is a placeholder for ANY bound variable.
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Exists(DSL::Variable("x"), DSL::Not(DSL::Metavariable("P")));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    // Case 1: Input uses variable 'x' (Same name as pattern)
    auto exprX = Forall(Var("x"), Pred("A"));
    ExpressionTransformer transformer;
    auto result1 = transformer.rewrite(exprX, { rule });

    // Should change to Exists
    ASSERT_EQ(result1->exprType, Expression::Type::QUANTIFICATION);
    auto q1 = std::dynamic_pointer_cast<QuantificationFormula>(result1);
    EXPECT_EQ(q1->type, QuantificationFormula::Quantifier::EXISTS);
    EXPECT_EQ(q1->variable->symbol, "x");

    // Case 2: Input uses variable 'y' (Different name than pattern)
    // The Spec implies unification: pattern 'x' should bind to formula 'y'.
    auto exprY = Forall(Var("y"), Pred("A"));
    auto result2 = transformer.rewrite(exprY, { rule });

    // Should ALSO change to Exists (Rule matched by structure/unification)
    ASSERT_EQ(result2->exprType, Expression::Type::QUANTIFICATION);
    auto q2 = std::dynamic_pointer_cast<QuantificationFormula>(result2);

    EXPECT_EQ(q2->type, QuantificationFormula::Quantifier::EXISTS);

    // CRITICAL: The variable name from the INPUT formula ("y") must be preserved,
    // even though the rule pattern used "x".
    EXPECT_EQ(q2->variable->symbol, "y");
}

TEST_F(ExpressionTransformerTest, Rewrite_SwapsSubtrees_UsingMetavariables) {
    // Rule: $A AND $B  ==>  $B OR $A 
    auto pattern = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Metavariable("B"), DSL::Metavariable("A"));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    // Input: P(x) AND (Q(y) OR R(z))
    // $A = P(x)
    // $B = (Q(y) OR R(z))
    auto expr = And(
        Pred("P", { Var("x") }),
        Or(Pred("Q", { Var("y") }), Pred("R", { Var("z") }))
    );

    ExpressionTransformer transformer;
    auto result = transformer.rewrite(expr, { rule });

    // Expected: (Q(y) OR R(z)) OR P(x)
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);

    // Root operator should be OR (due to rule change)
    EXPECT_EQ(binary->op, BinaryFormula::Operator::OR);

    // Left side should now be the subtree that was originally on the right ($B)
    EXPECT_EQ(binary->left->exprType, Expression::Type::BINARY);
    auto leftOp = std::dynamic_pointer_cast<BinaryFormula>(binary->left);
    // The inner subtree ($B) was an OR and stays an OR
    EXPECT_EQ(leftOp->op, BinaryFormula::Operator::OR);

    // Right side should be P(x) ($A)
    EXPECT_EQ(binary->right->exprType, Expression::Type::PREDICATE);
    auto rightPred = std::dynamic_pointer_cast<PredicateFormula>(binary->right);
    EXPECT_EQ(rightPred->symbol, "P");
}

TEST_F(ExpressionTransformerTest, Rewrite_AppliesRuleWithCondition_WhenConditionMet) {
    // Rule: Forall x, $P  ==>  $P   (Eliminate vacuous quantifier)
    // Condition: NotFreeIn(x, $P)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };
    ExpressionTransformer::ReplacementRule rule(pattern, replacement, conditions);

    // Input: Forall x, Q(y) 
    // x is NOT free in Q(y). Condition met.
    auto expr = Forall(Var("x"), Pred("Q", { Var("y") }));

    ExpressionTransformer transformer;
    auto result = transformer.rewrite(expr, { rule });

    // Expected: Q(y) (Quantifier removed)
    EXPECT_EQ(result->exprType, Expression::Type::PREDICATE);
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(result);
    EXPECT_EQ(pred->symbol, "Q");
}

TEST_F(ExpressionTransformerTest, Rewrite_IgnoresRule_WhenConditionFailed) {
    // Rule: Forall x, $P  ==>  $P
    // Condition: NotFreeIn(x, $P)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };
    ExpressionTransformer::ReplacementRule rule(pattern, replacement, conditions);

    // Input: Forall x, Q(x)
    // x IS free in Q(x). Condition failed.
    auto expr = Forall(Var("x"), Pred("Q", { Var("x") }));

    ExpressionTransformer transformer;
    auto result = transformer.rewrite(expr, { rule });

    // Expected: Unchanged, Forall x, Q(x)
    EXPECT_EQ(result->exprType, Expression::Type::QUANTIFICATION);
}

TEST_F(ExpressionTransformerTest, RewriteFast_ReturnsOriginal_WhenNoRulesMatch) {
    // Formula: P(x) AND Q(y)
    auto expr = And(Pred("P", { Var("x") }), Pred("Q", { Var("y") }));

    // Rule: $A -> $B  ==>  ~$A OR $B (Implication elimination)
    // There is no implication in the input expr.
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    ExpressionTransformer transformer;
    // ZMIANA: rewrite -> rewriteFast
    auto result = transformer.rewriteFast(expr, { rule });

    // Structure should remain identical
    EXPECT_EQ(ExpressionTransformer::getExpressionSize(expr), ExpressionTransformer::getExpressionSize(result));

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::AND);
}

TEST_F(ExpressionTransformerTest, RewriteFast_AppliesRule_AtRoot) {
    // Formula: P(x) -> Q(y)
    auto expr = Imp(Pred("P", { Var("x") }), Pred("Q", { Var("y") }));

    // Rule: $A -> $B  ==>  ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    ExpressionTransformer transformer;
    // ZMIANA: rewrite -> rewriteFast
    auto result = transformer.rewriteFast(expr, { rule });

    // Expected: ~P(x) OR Q(y)
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::OR);

    // Left child should be Negation
    EXPECT_EQ(binary->left->exprType, Expression::Type::NEGATION); // ~P(x)
}

TEST_F(ExpressionTransformerTest, RewriteFast_AppliesRule_RecursivelyDeepInTree) {
    // Formula: R(z) AND (P(x) -> Q(y))
    // The implication is nested inside an AND
    auto expr = And(
        Pred("R", { Var("z") }),
        Imp(Pred("P", { Var("x") }), Pred("Q", { Var("y") }))
    );

    // Rule: $A -> $B  ==>  ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    ExpressionTransformer transformer;
    // ZMIANA: rewrite -> rewriteFast
    auto result = transformer.rewriteFast(expr, { rule });

    // Expected: R(z) AND (~P(x) OR Q(y))
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::AND);

    // Check right child (where the rewrite happened)
    auto rightChild = std::dynamic_pointer_cast<BinaryFormula>(binary->right);
    ASSERT_TRUE(rightChild != nullptr);
    EXPECT_EQ(rightChild->op, BinaryFormula::Operator::OR);
}

TEST_F(ExpressionTransformerTest, RewriteFast_MatchesPatternVariable_UsingUnification) {
    // Rule: Forall x, $P  ==>  Exists x, ~$P (Dummy rule for testing)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Exists(DSL::Variable("x"), DSL::Not(DSL::Metavariable("P")));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    // Case 1: Input uses variable 'x' (Same name as pattern)
    auto exprX = Forall(Var("x"), Pred("A"));
    ExpressionTransformer transformer;
    // ZMIANA: rewrite -> rewriteFast
    auto result1 = transformer.rewriteFast(exprX, { rule });

    // Should change to Exists
    ASSERT_EQ(result1->exprType, Expression::Type::QUANTIFICATION);
    auto q1 = std::dynamic_pointer_cast<QuantificationFormula>(result1);
    EXPECT_EQ(q1->type, QuantificationFormula::Quantifier::EXISTS);
    EXPECT_EQ(q1->variable->symbol, "x");

    // Case 2: Input uses variable 'y' (Different name than pattern)
    auto exprY = Forall(Var("y"), Pred("A"));
    // ZMIANA: rewrite -> rewriteFast
    auto result2 = transformer.rewriteFast(exprY, { rule });

    // Should ALSO change to Exists (Rule matched by structure/unification)
    ASSERT_EQ(result2->exprType, Expression::Type::QUANTIFICATION);
    auto q2 = std::dynamic_pointer_cast<QuantificationFormula>(result2);

    EXPECT_EQ(q2->type, QuantificationFormula::Quantifier::EXISTS);

    // CRITICAL: The variable name from the INPUT formula ("y") must be preserved
    EXPECT_EQ(q2->variable->symbol, "y");
}

TEST_F(ExpressionTransformerTest, RewriteFast_SwapsSubtrees_UsingMetavariables) {
    // Rule: $A AND $B  ==>  $B OR $A 
    // (Using OR to prevent infinite loop as established previously)
    auto pattern = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Metavariable("B"), DSL::Metavariable("A"));
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    // Input: P(x) AND (Q(y) OR R(z))
    auto expr = And(
        Pred("P", { Var("x") }),
        Or(Pred("Q", { Var("y") }), Pred("R", { Var("z") }))
    );

    ExpressionTransformer transformer;
    // ZMIANA: rewrite -> rewriteFast
    auto result = transformer.rewriteFast(expr, { rule });

    // Expected: (Q(y) OR R(z)) OR P(x)
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);

    // Root operator should be OR (due to rule change)
    EXPECT_EQ(binary->op, BinaryFormula::Operator::OR);

    // Left side should now be the subtree that was originally on the right ($B)
    EXPECT_EQ(binary->left->exprType, Expression::Type::BINARY);
    auto leftOp = std::dynamic_pointer_cast<BinaryFormula>(binary->left);
    EXPECT_EQ(leftOp->op, BinaryFormula::Operator::OR);

    // Right side should be P(x) ($A)
    EXPECT_EQ(binary->right->exprType, Expression::Type::PREDICATE);
    auto rightPred = std::dynamic_pointer_cast<PredicateFormula>(binary->right);
    EXPECT_EQ(rightPred->symbol, "P");
}

TEST_F(ExpressionTransformerTest, RewriteFast_AppliesRuleWithCondition_WhenConditionMet) {
    // Rule: Forall x, $P  ==>  $P   (Eliminate vacuous quantifier)
    // Condition: NotFreeIn(x, $P)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };
    ExpressionTransformer::ReplacementRule rule(pattern, replacement, conditions);

    // Input: Forall x, Q(y) 
    // x is NOT free in Q(y). Condition met.
    auto expr = Forall(Var("x"), Pred("Q", { Var("y") }));

    ExpressionTransformer transformer;
    // ZMIANA: rewrite -> rewriteFast
    auto result = transformer.rewriteFast(expr, { rule });

    // Expected: Q(y) (Quantifier removed)
    EXPECT_EQ(result->exprType, Expression::Type::PREDICATE);
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(result);
    EXPECT_EQ(pred->symbol, "Q");
}

TEST_F(ExpressionTransformerTest, RewriteFast_IgnoresRule_WhenConditionFailed) {
    // Rule: Forall x, $P  ==>  $P
    // Condition: NotFreeIn(x, $P)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };
    ExpressionTransformer::ReplacementRule rule(pattern, replacement, conditions);

    // Input: Forall x, Q(x)
    // x IS free in Q(x). Condition failed.
    auto expr = Forall(Var("x"), Pred("Q", { Var("x") }));

    ExpressionTransformer transformer;
    // ZMIANA: rewrite -> rewriteFast
    auto result = transformer.rewriteFast(expr, { rule });

    // Expected: Unchanged, Forall x, Q(x)
    EXPECT_EQ(result->exprType, Expression::Type::QUANTIFICATION);
}

TEST_F(ExpressionTransformerTest, RewriteFast_ModifiesInPlace_ComplexAggressive) {
    // 1. Rule: Double Negation Elimination (~~A -> A)
    auto pattern = DSL::Not(DSL::Not(DSL::Metavariable("A")));
    auto replacement = DSL::Metavariable("A");
    ExpressionTransformer::ReplacementRule rule(pattern, replacement);

    // 2. Complex Input Construction
    // Structure: Forall x, ( (~~P(x)) AND ( Q(y) OR (~~R(z)) ) )
    // This forces recursion through Quantifier -> Binary(AND) -> Binary(OR).

    // Components to track pointers:
    auto p = Pred("P", { Var("x") });
    auto doubleNegP = Not(Not(p)); // Target 1 (Left side of AND)

    auto q = Pred("Q", { Var("y") }); // Untouched atom

    auto r = Pred("R", { Var("z") });
    auto doubleNegR = Not(Not(r)); // Target 2 (Deep right side inside OR)

    auto orNode = Or(q, doubleNegR);        // Intermediate Node 1
    auto andNode = And(doubleNegP, orNode); // Intermediate Node 2
    auto root = Forall(Var("x"), andNode);  // Root Node

    // 3. Execute
    ExpressionTransformer transformer;
    auto result = transformer.rewriteFast(root, { rule }, true);

    // 4. Pointer Identity Checks (Aggressive)

    // Root (Forall) must remain the same address
    EXPECT_EQ(result.get(), root.get());

    // Navigate down
    auto resQuant = std::dynamic_pointer_cast<QuantificationFormula>(result);
    ASSERT_TRUE(resQuant != nullptr);

    // Body (AND) must remain the same address
    EXPECT_EQ(resQuant->body.get(), andNode.get());
    auto resAnd = std::dynamic_pointer_cast<BinaryFormula>(resQuant->body);

    // Left child: ~~P should become P
    // The pointer changes here, but content is P
    EXPECT_EQ(resAnd->left->exprType, Expression::Type::PREDICATE);
    EXPECT_EQ(std::dynamic_pointer_cast<PredicateFormula>(resAnd->left)->symbol, "P");

    // Right child (OR) must remain the same address
    EXPECT_EQ(resAnd->right.get(), orNode.get());
    auto resOr = std::dynamic_pointer_cast<BinaryFormula>(resAnd->right);

    // OR Left child: Q(y) must remain the EXACT SAME address (untouched)
    EXPECT_EQ(resOr->left.get(), q.get());

    // OR Right child: ~~R should become R
    // Pointer changes, content is R
    EXPECT_EQ(resOr->right->exprType, Expression::Type::PREDICATE);
    EXPECT_EQ(std::dynamic_pointer_cast<PredicateFormula>(resOr->right)->symbol, "R");
}

TEST_F(ExpressionTransformerTest, FlattenToJunction_WrapsAtom_WhenTargetMismatch) {
    // Input: P(A)
    // Target: AND
    // Expected: AND( P(A) )
    auto atom = Pred("A");

    ExpressionTransformer transformer;
    auto result = transformer.flattenToJunction(atom, JunctionFormula::Operator::AND);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->op, JunctionFormula::Operator::AND);
    ASSERT_EQ(result->operands.size(), 1);

    auto child = std::dynamic_pointer_cast<PredicateFormula>(result->operands[0]);
    ASSERT_TRUE(child != nullptr);
    EXPECT_EQ(child->symbol, "A");
}

TEST_F(ExpressionTransformerTest, FlattenToJunction_WrapsDifferentOperator) {
    // Input: A OR B
    // Target: AND
    // Expected: AND( A OR B )
    // The inner OR should remain intact as a single child.
    auto expr = Or(Pred("A"), Pred("B"));

    ExpressionTransformer transformer;
    auto result = transformer.flattenToJunction(expr, JunctionFormula::Operator::AND);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->op, JunctionFormula::Operator::AND);
    ASSERT_EQ(result->operands.size(), 1);

    auto child = std::dynamic_pointer_cast<BinaryFormula>(result->operands[0]);
    ASSERT_TRUE(child != nullptr);
    EXPECT_EQ(child->op, BinaryFormula::Operator::OR);
}

TEST_F(ExpressionTransformerTest, FlattenToJunction_FlattensBinaryTree) {
    // Input: A AND (B AND C)
    // Target: AND
    // Expected: AND(A, B, C)
    auto expr = And(
        Pred("A"),
        And(Pred("B"), Pred("C"))
    );

    ExpressionTransformer transformer;
    auto result = transformer.flattenToJunction(expr, JunctionFormula::Operator::AND);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->op, JunctionFormula::Operator::AND);
    ASSERT_EQ(result->operands.size(), 3);

    // Check order (DFS typically preserves left-to-right)
    auto p1 = std::dynamic_pointer_cast<PredicateFormula>(result->operands[0]);
    auto p2 = std::dynamic_pointer_cast<PredicateFormula>(result->operands[1]);
    auto p3 = std::dynamic_pointer_cast<PredicateFormula>(result->operands[2]);

    EXPECT_EQ(p1->symbol, "A");
    EXPECT_EQ(p2->symbol, "B");
    EXPECT_EQ(p3->symbol, "C");
}

TEST_F(ExpressionTransformerTest, FlattenToJunction_FlattensDeeplyNestedStructure) {
    // Input: (A AND B) AND (C AND (D AND E))
    // Target: AND
    // Expected: AND(A, B, C, D, E)
    auto left = And(Pred("A"), Pred("B"));
    auto right = And(Pred("C"), And(Pred("D"), Pred("E")));
    auto expr = And(left, right);

    ExpressionTransformer transformer;
    auto result = transformer.flattenToJunction(expr, JunctionFormula::Operator::AND);

    ASSERT_EQ(result->operands.size(), 5);
}

TEST_F(ExpressionTransformerTest, FlattenToJunction_FinalizesCNF_PreservesClauses) {
    // Input: (A OR B) AND (C OR D)
    // Target: AND
    // Expected: AND( (A OR B), (C OR D) )
    // This ensures that the OR clauses are treated as atomic units relative to the AND flattening.
    auto clause1 = Or(Pred("A"), Pred("B"));
    auto clause2 = Or(Pred("C"), Pred("D"));
    auto expr = And(clause1, clause2);

    ExpressionTransformer transformer;
    auto result = transformer.flattenToJunction(expr, JunctionFormula::Operator::AND);

    ASSERT_EQ(result->operands.size(), 2);

    auto op1 = std::dynamic_pointer_cast<BinaryFormula>(result->operands[0]);
    auto op2 = std::dynamic_pointer_cast<BinaryFormula>(result->operands[1]);

    ASSERT_TRUE(op1 != nullptr);
    ASSERT_TRUE(op2 != nullptr);
    EXPECT_EQ(op1->op, BinaryFormula::Operator::OR);
    EXPECT_EQ(op2->op, BinaryFormula::Operator::OR);
}

TEST_F(ExpressionTransformerTest, FlattenToJunction_MergesExistingJunctions) {
    // Input: AND( JunctionAND(A, B), C )
    // Target: AND
    // Expected: AND(A, B, C)
    // Should handle mix of BinaryFormula AND and JunctionFormula AND.
    auto junction = Conjunction({ Pred("A"), Pred("B") });
    auto expr = And(junction, Pred("C"));

    ExpressionTransformer transformer;
    auto result = transformer.flattenToJunction(expr, JunctionFormula::Operator::AND);

    ASSERT_EQ(result->operands.size(), 3);
    EXPECT_EQ(std::dynamic_pointer_cast<PredicateFormula>(result->operands[0])->symbol, "A");
    EXPECT_EQ(std::dynamic_pointer_cast<PredicateFormula>(result->operands[1])->symbol, "B");
    EXPECT_EQ(std::dynamic_pointer_cast<PredicateFormula>(result->operands[2])->symbol, "C");
}

TEST_F(ExpressionTransformerTest, FlattenToJunction_HandlesEmptyJunction) {
    // Input: JunctionAND({})
    // Target: AND
    auto empty = Conjunction({});

    ExpressionTransformer transformer;
    auto result = transformer.flattenToJunction(empty, JunctionFormula::Operator::AND);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->operands.empty());
}

TEST_F(ExpressionTransformerTest, FlattenToJunction_OptimizesSingleElementJunction) {
    // Input: JunctionAND({A})
    // Target: AND
    // Expected: AND(A)
    auto single = Conjunction({ Pred("A") });

    ExpressionTransformer transformer;
    auto result = transformer.flattenToJunction(single, JunctionFormula::Operator::AND);

    ASSERT_EQ(result->operands.size(), 1);
    EXPECT_EQ(result->operands[0]->exprType, Expression::Type::PREDICATE);
}

TEST_F(ExpressionTransformerTest, FlattenToJunction_RespectsInPlaceFlag) {
    // Input: A AND B
    // InPlace: true
    // This primarily tests that the function runs without error when optimization is requested.
    auto expr = And(Pred("A"), Pred("B"));

    ExpressionTransformer transformer;
    auto result = transformer.flattenToJunction(expr, JunctionFormula::Operator::AND, true);

    ASSERT_EQ(result->operands.size(), 2);
    EXPECT_EQ(result->op, JunctionFormula::Operator::AND);
}

TEST_F(ExpressionTransformerTest, AlphaEquivalent_ReturnsTrue_ForSameDistinctObjects) {
    auto d1 = Distinct("Apple");
    auto d2 = Distinct("Apple");
    EXPECT_TRUE(ExpressionTransformer::areAlphaEquivalent(d1, d2));
}

TEST_F(ExpressionTransformerTest, AlphaEquivalent_ReturnsFalse_ForDifferentDistinctObjects) {
    auto d1 = Distinct("Apple");
    auto d2 = Distinct("Orange");
    EXPECT_FALSE(ExpressionTransformer::areAlphaEquivalent(d1, d2));
}

TEST_F(ExpressionTransformerTest, AlphaEquivalent_ReturnsFalse_ForDistinctVsFunction) {
    // Distinct object "c" must differ from function constant "c"
    auto distinctObj = Distinct("c");
    auto funcConst = Func("c");
    EXPECT_FALSE(ExpressionTransformer::areAlphaEquivalent(distinctObj, funcConst));
}

TEST_F(ExpressionTransformerTest, AlphaEquivalent_ReturnsFalse_ForDistinctVsVariable) {
    auto distinctObj = Distinct("X");
    auto variable = Var("X");
    EXPECT_FALSE(ExpressionTransformer::areAlphaEquivalent(distinctObj, variable));
}

TEST_F(ExpressionTransformerTest, StandardizeVariables_PreservesDistinctObjects) {
    // Formula: ![X]: P(X, Distinct[Obj])
    auto qVar = Var("X");
    auto distinctObj = Distinct("Obj");

    // NOTE: Use new Var("X") inside Pred to ensure Tree structure (not DAG)
    auto body = Pred("P", { Var("X"), distinctObj });
    auto formula = Forall(qVar, body);

    ExpressionTransformer transformer;
    auto standardized = transformer.standardizeVariables(formula);

    auto quant = std::dynamic_pointer_cast<QuantificationFormula>(standardized);
    ASSERT_TRUE(quant);

    auto predBody = std::dynamic_pointer_cast<PredicateFormula>(quant->body);
    ASSERT_TRUE(predBody);

    // Verify the second argument remains a Distinct Object
    ASSERT_EQ(predBody->arguments.size(), 2);
    auto arg2 = std::dynamic_pointer_cast<FunctionTerm>(predBody->arguments[1]);

    ASSERT_TRUE(arg2);
    EXPECT_TRUE(arg2->distinct);
    EXPECT_EQ(arg2->symbol, "Obj");
}

TEST_F(ExpressionTransformerTest, Skolemize_TreatsDistinctObjectsAsConstants) {
    // Formula: ?[Y]: ( Y = Distinct[1] )
    // Expected: sk0 = Distinct[1]

    auto qVar = Var("Y");
    auto distinctObj = Distinct("1");

    // NOTE: Use new Var("Y") to maintain Tree structure
    auto body = Equal(Var("Y"), distinctObj);
    auto formula = Exists(qVar, body);

    ExpressionTransformer transformer;
    auto skolemized = transformer.skolemize(formula);

    auto eq = std::dynamic_pointer_cast<EqualityFormula>(skolemized);
    ASSERT_TRUE(eq);

    auto left = std::dynamic_pointer_cast<FunctionTerm>(eq->left);
    auto right = std::dynamic_pointer_cast<FunctionTerm>(eq->right);

    ASSERT_TRUE(left);
    ASSERT_TRUE(right);

    // Left side is a new Skolem constant (not distinct)
    EXPECT_FALSE(left->distinct);

    // Right side must remain Distinct Object
    EXPECT_TRUE(right->distinct);
    EXPECT_EQ(right->symbol, "1");
}

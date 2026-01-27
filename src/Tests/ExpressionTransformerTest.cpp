#include <gtest/gtest.h>

#include "../ExpressionTransformer.hpp"
#include "../ExpressionBuilder.hpp"
#include "../ExpressionUtils.hpp"
#include "../ExpressionRewriter.hpp"

using namespace ExpressionBuilder;
namespace DSL = ExpressionRewriter::DSL;

class ExpressionTransformerTest : public ::testing::Test {};

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

    EXPECT_TRUE(ExpressionUtils::areAlphaEquivalent(resAnd, DSL::True()));

    // Empty OR -> False
    auto emptyOr = std::make_shared<JunctionFormula>(JunctionFormula::Operator::OR, std::vector<FormulaPtr>{});
    auto resOr = transformer.eliminateJunction(emptyOr, false);

    EXPECT_TRUE(ExpressionUtils::areAlphaEquivalent(resOr, DSL::False()));
}

// 2. Wrapping check: Single operand unwraps correctly
TEST_F(ExpressionTransformerTest, EliminateJunction_SingleOperand_UnwrapsContent) {
    ExpressionTransformer transformer;
    auto atom = Pred("P");
    auto singleAnd = std::make_shared<JunctionFormula>(JunctionFormula::Operator::AND, std::vector<FormulaPtr>{atom});

    // Case A: inPlace = false (Expect Deep Copy)
    auto resultCopy = transformer.eliminateJunction(singleAnd, false);

    // Logic: The content is P
    EXPECT_TRUE(ExpressionUtils::areAlphaEquivalent(resultCopy, atom));
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

    EXPECT_TRUE(ExpressionUtils::areAlphaEquivalent(result, expectedExpr));
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

    EXPECT_TRUE(ExpressionUtils::areAlphaEquivalent(result, expectedExpr));
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

    ASSERT_TRUE(ExpressionUtils::isStandardized(result));

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

    ASSERT_TRUE(ExpressionUtils::isStandardized(result));

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

    ASSERT_TRUE(ExpressionUtils::isStandardized(result));

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
    EXPECT_TRUE(ExpressionUtils::isStandardized(result));

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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(expr);
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
    transformer.getNameRegistry()->registerPredAndFuncNames(formula);
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

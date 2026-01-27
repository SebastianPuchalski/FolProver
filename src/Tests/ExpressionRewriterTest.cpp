#include <gtest/gtest.h>

#include "../ExpressionRewriter.hpp"
#include "../ExpressionBuilder.hpp"
#include "../ExpressionUtils.hpp" 

using namespace ExpressionBuilder;
namespace DSL = ExpressionRewriter::DSL;

class ExpressionRewriterTest : public ::testing::Test {};

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsTrue_ForValidImplicationRule) {
    // Pattern: $A -> $B
    // Replacement: ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsTrue_ForValidQuantifierRule) {
    // Pattern: Forall x, $P
    // Replacement: Exists x, $P
    // Valid because 'x' and 'P' are defined in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Exists(DSL::Variable("x"), DSL::Metavariable("P"));

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsTrue_ForMultiUseInReplacement) {
    // Pattern: $A
    // Replacement: $A AND $A
    // Valid to reuse metavariables in replacement
    auto pattern = DSL::Metavariable("A");
    auto replacement = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("A"));

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsTrue_ForSwappedMetavariables) {
    // Pattern: $A AND $B
    // Replacement: $B AND $A
    auto pattern = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::And(DSL::Metavariable("B"), DSL::Metavariable("A"));

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsFalse_ForDuplicateMetavariableInPattern) {
    // Pattern: $A AND $A  <-- Invalid: Metavariable 'A' appears twice
    // Replacement: $A
    auto pattern = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("A"));
    auto replacement = DSL::Metavariable("A");

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_FALSE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsFalse_ForUnknownMetavariableInReplacement) {
    // Pattern: $A
    // Replacement: $B <-- Invalid: 'B' was not in pattern
    auto pattern = DSL::Metavariable("A");
    auto replacement = DSL::Metavariable("B");

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_FALSE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsFalse_ForUnknownVariableInReplacement) {
    // Pattern: Forall x, $P
    // Replacement: Forall y, $P <-- Invalid: 'y' was not in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Forall(DSL::Variable("y"), DSL::Metavariable("P"));

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_FALSE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsTrue_ForValidCondition) {
    // Pattern: Forall x, $P
    // Condition: NotFreeIn(x, $P)
    // Valid: both x and P are in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };

    ExpressionRewriter::ReplacementRule rule(pattern, replacement, conditions);
    EXPECT_TRUE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsFalse_ForUnknownVariableInCondition) {
    // Condition uses 'y', which is not in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("y"), DSL::Metavariable("P"))
    };

    ExpressionRewriter::ReplacementRule rule(pattern, replacement, conditions);
    EXPECT_FALSE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsFalse_ForUnknownMetavariableInCondition) {
    // Condition uses 'Q', which is not in pattern
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("Q"))
    };

    ExpressionRewriter::ReplacementRule rule(pattern, replacement, conditions);
    EXPECT_FALSE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsTrue_ForRepeatedQuantifierVariablesInPattern) {
    // Specification: "Quantifier variables can appear multiple times"
    // Pattern: (Forall x, $A) AND (Exists x, $B) -> 'x' is reused, which is valid for variables
    auto pattern = DSL::And(
        DSL::Forall(DSL::Variable("x"), DSL::Metavariable("A")),
        DSL::Exists(DSL::Variable("x"), DSL::Metavariable("B"))
    );

    auto replacement = DSL::Metavariable("A");

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsTrue_ForBooleanConstantsAndExtendedOps) {
    // Specification: Allowed elements include True, False, Eqv, Xor
    // Pattern: $A Xor False
    auto pattern = DSL::Xor(DSL::Metavariable("A"), DSL::False());

    // Replacement: $A Eqv True
    auto replacement = DSL::Eqv(DSL::Metavariable("A"), DSL::True());

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, IsReplacementRuleCorrect_ReturnsTrue_ForComplexDeeplyNestedPattern) {
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

    ExpressionRewriter::ReplacementRule rule(pattern, replacement);
    EXPECT_TRUE(ExpressionRewriter::isReplacementRuleCorrect(rule));
}

TEST_F(ExpressionRewriterTest, AreReplacementRulesCorrect_ReturnsTrue_ForVectorOfValidRules) {
    auto p1 = DSL::Metavariable("A");
    auto r1 = DSL::Metavariable("A");
    ExpressionRewriter::ReplacementRule rule1(p1, r1);

    auto p2 = DSL::Imp(DSL::Metavariable("B"), DSL::False());
    auto r2 = DSL::Not(DSL::Metavariable("B"));
    ExpressionRewriter::ReplacementRule rule2(p2, r2);

    std::vector<ExpressionRewriter::ReplacementRule> rules = { rule1, rule2 };
    EXPECT_TRUE(ExpressionRewriter::areReplacementRulesCorrect(rules));
}

TEST_F(ExpressionRewriterTest, AreReplacementRulesCorrect_ReturnsFalse_IfOneRuleIsInvalid) {
    // Rule 1: Valid
    ExpressionRewriter::ReplacementRule validRule(DSL::Metavariable("A"), DSL::Metavariable("A"));

    // Rule 2: Invalid (Replacement uses undefined metavariable 'B')
    ExpressionRewriter::ReplacementRule invalidRule(DSL::Metavariable("A"), DSL::Metavariable("B"));

    std::vector<ExpressionRewriter::ReplacementRule> rules = { validRule, invalidRule };
    EXPECT_FALSE(ExpressionRewriter::areReplacementRulesCorrect(rules));
}

TEST_F(ExpressionRewriterTest, Rewrite_ReturnsOriginal_WhenNoRulesMatch) {
    // Formula: P(x) AND Q(y)
    auto expr = And(Pred("P", { Var("x") }), Pred("Q", { Var("y") }));

    // Rule: $A -> $B  ==>  ~$A OR $B (Implication elimination)
    // There is no implication in the input expr.
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    auto result = ExpressionRewriter::rewrite(expr, { rule });

    // Structure should remain identical
    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr), ExpressionUtils::getExpressionSize(result));

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::AND);
}

TEST_F(ExpressionRewriterTest, Rewrite_AppliesRule_AtRoot) {
    // Formula: P(x) -> Q(y)
    auto expr = Imp(Pred("P", { Var("x") }), Pred("Q", { Var("y") }));

    // Rule: $A -> $B  ==>  ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    auto result = ExpressionRewriter::rewrite(expr, { rule });

    // Expected: ~P(x) OR Q(y)
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::OR);

    // Left child should be Negation
    EXPECT_EQ(binary->left->exprType, Expression::Type::NEGATION); // ~P(x)
}

TEST_F(ExpressionRewriterTest, Rewrite_AppliesRule_RecursivelyDeepInTree) {
    // Formula: R(z) AND (P(x) -> Q(y))
    // The implication is nested inside an AND
    auto expr = And(
        Pred("R", { Var("z") }),
        Imp(Pred("P", { Var("x") }), Pred("Q", { Var("y") }))
    );

    // Rule: $A -> $B  ==>  ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    auto result = ExpressionRewriter::rewrite(expr, { rule });

    // Expected: R(z) AND (~P(x) OR Q(y))
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::AND);

    // Check right child (where the rewrite happened)
    auto rightChild = std::dynamic_pointer_cast<BinaryFormula>(binary->right);
    ASSERT_TRUE(rightChild != nullptr);
    EXPECT_EQ(rightChild->op, BinaryFormula::Operator::OR);
}

TEST_F(ExpressionRewriterTest, Rewrite_MatchesPatternVariable_UsingUnification) {
    // Rule: Forall x, $P  ==>  Exists x, ~$P (Dummy rule for testing)
    // In Pattern Matching logic, 'x' here is a placeholder for ANY bound variable.
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Exists(DSL::Variable("x"), DSL::Not(DSL::Metavariable("P")));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    // Case 1: Input uses variable 'x' (Same name as pattern)
    auto exprX = Forall(Var("x"), Pred("A"));
    auto result1 = ExpressionRewriter::rewrite(exprX, { rule });

    // Should change to Exists
    ASSERT_EQ(result1->exprType, Expression::Type::QUANTIFICATION);
    auto q1 = std::dynamic_pointer_cast<QuantificationFormula>(result1);
    EXPECT_EQ(q1->type, QuantificationFormula::Quantifier::EXISTS);
    EXPECT_EQ(q1->variable->symbol, "x");

    // Case 2: Input uses variable 'y' (Different name than pattern)
    // The Spec implies unification: pattern 'x' should bind to formula 'y'.
    auto exprY = Forall(Var("y"), Pred("A"));
    auto result2 = ExpressionRewriter::rewrite(exprY, { rule });

    // Should ALSO change to Exists (Rule matched by structure/unification)
    ASSERT_EQ(result2->exprType, Expression::Type::QUANTIFICATION);
    auto q2 = std::dynamic_pointer_cast<QuantificationFormula>(result2);

    EXPECT_EQ(q2->type, QuantificationFormula::Quantifier::EXISTS);

    // CRITICAL: The variable name from the INPUT formula ("y") must be preserved,
    // even though the rule pattern used "x".
    EXPECT_EQ(q2->variable->symbol, "y");
}

TEST_F(ExpressionRewriterTest, Rewrite_SwapsSubtrees_UsingMetavariables) {
    // Rule: $A AND $B  ==>  $B OR $A 
    auto pattern = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Metavariable("B"), DSL::Metavariable("A"));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    // Input: P(x) AND (Q(y) OR R(z))
    // $A = P(x)
    // $B = (Q(y) OR R(z))
    auto expr = And(
        Pred("P", { Var("x") }),
        Or(Pred("Q", { Var("y") }), Pred("R", { Var("z") }))
    );

    auto result = ExpressionRewriter::rewrite(expr, { rule });

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

TEST_F(ExpressionRewriterTest, Rewrite_AppliesRuleWithCondition_WhenConditionMet) {
    // Rule: Forall x, $P  ==>  $P   (Eliminate vacuous quantifier)
    // Condition: NotFreeIn(x, $P)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };
    ExpressionRewriter::ReplacementRule rule(pattern, replacement, conditions);

    // Input: Forall x, Q(y) 
    // x is NOT free in Q(y). Condition met.
    auto expr = Forall(Var("x"), Pred("Q", { Var("y") }));

    auto result = ExpressionRewriter::rewrite(expr, { rule });

    // Expected: Q(y) (Quantifier removed)
    EXPECT_EQ(result->exprType, Expression::Type::PREDICATE);
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(result);
    EXPECT_EQ(pred->symbol, "Q");
}

TEST_F(ExpressionRewriterTest, Rewrite_IgnoresRule_WhenConditionFailed) {
    // Rule: Forall x, $P  ==>  $P
    // Condition: NotFreeIn(x, $P)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };
    ExpressionRewriter::ReplacementRule rule(pattern, replacement, conditions);

    // Input: Forall x, Q(x)
    // x IS free in Q(x). Condition failed.
    auto expr = Forall(Var("x"), Pred("Q", { Var("x") }));

    auto result = ExpressionRewriter::rewrite(expr, { rule });

    // Expected: Unchanged, Forall x, Q(x)
    EXPECT_EQ(result->exprType, Expression::Type::QUANTIFICATION);
}

TEST_F(ExpressionRewriterTest, RewriteFast_ReturnsOriginal_WhenNoRulesMatch) {
    // Formula: P(x) AND Q(y)
    auto expr = And(Pred("P", { Var("x") }), Pred("Q", { Var("y") }));

    // Rule: $A -> $B  ==>  ~$A OR $B (Implication elimination)
    // There is no implication in the input expr.
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    auto result = ExpressionRewriter::rewriteFast(expr, { rule });

    // Structure should remain identical
    EXPECT_EQ(ExpressionUtils::getExpressionSize(expr), ExpressionUtils::getExpressionSize(result));

    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::AND);
}

TEST_F(ExpressionRewriterTest, RewriteFast_AppliesRule_AtRoot) {
    // Formula: P(x) -> Q(y)
    auto expr = Imp(Pred("P", { Var("x") }), Pred("Q", { Var("y") }));

    // Rule: $A -> $B  ==>  ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    auto result = ExpressionRewriter::rewriteFast(expr, { rule });

    // Expected: ~P(x) OR Q(y)
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::OR);

    // Left child should be Negation
    EXPECT_EQ(binary->left->exprType, Expression::Type::NEGATION); // ~P(x)
}

TEST_F(ExpressionRewriterTest, RewriteFast_AppliesRule_RecursivelyDeepInTree) {
    // Formula: R(z) AND (P(x) -> Q(y))
    // The implication is nested inside an AND
    auto expr = And(
        Pred("R", { Var("z") }),
        Imp(Pred("P", { Var("x") }), Pred("Q", { Var("y") }))
    );

    // Rule: $A -> $B  ==>  ~$A OR $B
    auto pattern = DSL::Imp(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Not(DSL::Metavariable("A")), DSL::Metavariable("B"));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    auto result = ExpressionRewriter::rewriteFast(expr, { rule });

    // Expected: R(z) AND (~P(x) OR Q(y))
    auto binary = std::dynamic_pointer_cast<BinaryFormula>(result);
    ASSERT_TRUE(binary != nullptr);
    EXPECT_EQ(binary->op, BinaryFormula::Operator::AND);

    // Check right child (where the rewrite happened)
    auto rightChild = std::dynamic_pointer_cast<BinaryFormula>(binary->right);
    ASSERT_TRUE(rightChild != nullptr);
    EXPECT_EQ(rightChild->op, BinaryFormula::Operator::OR);
}

TEST_F(ExpressionRewriterTest, RewriteFast_MatchesPatternVariable_UsingUnification) {
    // Rule: Forall x, $P  ==>  Exists x, ~$P (Dummy rule for testing)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Exists(DSL::Variable("x"), DSL::Not(DSL::Metavariable("P")));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    // Case 1: Input uses variable 'x' (Same name as pattern)
    auto exprX = Forall(Var("x"), Pred("A"));
    auto result1 = ExpressionRewriter::rewriteFast(exprX, { rule });

    // Should change to Exists
    ASSERT_EQ(result1->exprType, Expression::Type::QUANTIFICATION);
    auto q1 = std::dynamic_pointer_cast<QuantificationFormula>(result1);
    EXPECT_EQ(q1->type, QuantificationFormula::Quantifier::EXISTS);
    EXPECT_EQ(q1->variable->symbol, "x");

    // Case 2: Input uses variable 'y' (Different name than pattern)
    auto exprY = Forall(Var("y"), Pred("A"));
    auto result2 = ExpressionRewriter::rewriteFast(exprY, { rule });

    // Should ALSO change to Exists (Rule matched by structure/unification)
    ASSERT_EQ(result2->exprType, Expression::Type::QUANTIFICATION);
    auto q2 = std::dynamic_pointer_cast<QuantificationFormula>(result2);

    EXPECT_EQ(q2->type, QuantificationFormula::Quantifier::EXISTS);

    // CRITICAL: The variable name from the INPUT formula ("y") must be preserved
    EXPECT_EQ(q2->variable->symbol, "y");
}

TEST_F(ExpressionRewriterTest, RewriteFast_SwapsSubtrees_UsingMetavariables) {
    // Rule: $A AND $B  ==>  $B OR $A 
    // (Using OR to prevent infinite loop as established previously)
    auto pattern = DSL::And(DSL::Metavariable("A"), DSL::Metavariable("B"));
    auto replacement = DSL::Or(DSL::Metavariable("B"), DSL::Metavariable("A"));
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

    // Input: P(x) AND (Q(y) OR R(z))
    auto expr = And(
        Pred("P", { Var("x") }),
        Or(Pred("Q", { Var("y") }), Pred("R", { Var("z") }))
    );

    auto result = ExpressionRewriter::rewriteFast(expr, { rule });

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

TEST_F(ExpressionRewriterTest, RewriteFast_AppliesRuleWithCondition_WhenConditionMet) {
    // Rule: Forall x, $P  ==>  $P   (Eliminate vacuous quantifier)
    // Condition: NotFreeIn(x, $P)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };
    ExpressionRewriter::ReplacementRule rule(pattern, replacement, conditions);

    // Input: Forall x, Q(y) 
    // x is NOT free in Q(y). Condition met.
    auto expr = Forall(Var("x"), Pred("Q", { Var("y") }));

    auto result = ExpressionRewriter::rewriteFast(expr, { rule });

    // Expected: Q(y) (Quantifier removed)
    EXPECT_EQ(result->exprType, Expression::Type::PREDICATE);
    auto pred = std::dynamic_pointer_cast<PredicateFormula>(result);
    EXPECT_EQ(pred->symbol, "Q");
}

TEST_F(ExpressionRewriterTest, RewriteFast_IgnoresRule_WhenConditionFailed) {
    // Rule: Forall x, $P  ==>  $P
    // Condition: NotFreeIn(x, $P)
    auto pattern = DSL::Forall(DSL::Variable("x"), DSL::Metavariable("P"));
    auto replacement = DSL::Metavariable("P");

    std::vector<DSL::Condition> conditions = {
        DSL::NotFreeIn(DSL::Variable("x"), DSL::Metavariable("P"))
    };
    ExpressionRewriter::ReplacementRule rule(pattern, replacement, conditions);

    // Input: Forall x, Q(x)
    // x IS free in Q(x). Condition failed.
    auto expr = Forall(Var("x"), Pred("Q", { Var("x") }));

    auto result = ExpressionRewriter::rewriteFast(expr, { rule });

    // Expected: Unchanged, Forall x, Q(x)
    EXPECT_EQ(result->exprType, Expression::Type::QUANTIFICATION);
}

TEST_F(ExpressionRewriterTest, RewriteFast_ModifiesInPlace_ComplexAggressive) {
    // 1. Rule: Double Negation Elimination (~~A -> A)
    auto pattern = DSL::Not(DSL::Not(DSL::Metavariable("A")));
    auto replacement = DSL::Metavariable("A");
    ExpressionRewriter::ReplacementRule rule(pattern, replacement);

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
    auto result = ExpressionRewriter::rewriteFast(root, { rule }, true);

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

#include <gtest/gtest.h>

#include "../Expression.hpp"
#include "../ExpressionBuilder.hpp"
#include "../ExpressionSerializer.hpp"

using namespace ExpressionBuilder;

// Helper: Deep equality check for Expressions
bool areEqual(const ExpressionPtr& a, const ExpressionPtr& b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->exprType != b->exprType) return false;

    // Check specific attributes
    switch (a->exprType) {
    case Expression::Type::BOOLEAN: {
        auto fa = std::static_pointer_cast<BooleanFormula>(a);
        auto fb = std::static_pointer_cast<BooleanFormula>(b);
        if (fa->value != fb->value) return false;
        break;
    }
    case Expression::Type::BINARY: {
        auto fa = std::static_pointer_cast<BinaryFormula>(a);
        auto fb = std::static_pointer_cast<BinaryFormula>(b);
        if (fa->op != fb->op) return false;
        break;
    }
    case Expression::Type::JUNCTION: {
        auto fa = std::static_pointer_cast<JunctionFormula>(a);
        auto fb = std::static_pointer_cast<JunctionFormula>(b);
        if (fa->op != fb->op) return false;
        break;
    }
    case Expression::Type::QUANTIFICATION: {
        auto fa = std::static_pointer_cast<QuantificationFormula>(a);
        auto fb = std::static_pointer_cast<QuantificationFormula>(b);
        if (fa->type != fb->type) return false;
        // Recursive check for the variable term
        if (!areEqual(fa->variable, fb->variable)) return false;
        break;
    }
    case Expression::Type::PREDICATE: {
        auto fa = std::static_pointer_cast<PredicateFormula>(a);
        auto fb = std::static_pointer_cast<PredicateFormula>(b);
        if (fa->symbol != fb->symbol) return false;
        break;
    }
    case Expression::Type::FUNCTION: {
        auto ta = std::static_pointer_cast<FunctionTerm>(a);
        auto tb = std::static_pointer_cast<FunctionTerm>(b);
        if (ta->symbol != tb->symbol) return false;
        break;
    }
    case Expression::Type::VARIABLE: {
        auto ta = std::static_pointer_cast<VariableTerm>(a);
        auto tb = std::static_pointer_cast<VariableTerm>(b);
        if (ta->symbol != tb->symbol) return false;
        break;
    }
    default: break;
    }

    // Check children recursively
    if (a->getChildCount() != b->getChildCount()) return false;
    for (size_t i = 0; i < a->getChildCount(); ++i) {
        if (!areEqual(a->getChild(i), b->getChild(i))) return false;
    }

    return true;
}

class ExpressionSerializerTest : public ::testing::Test {
protected:
    const ExpressionSerializer& serializer = ExpressionSerializer::getDefault();

    void verifyRoundTrip(const ExpressionPtr& original, const std::string& expectedString) {
        std::string serialized = serializer.serialize(original);
        EXPECT_EQ(serialized, expectedString) << "Serialization mismatch";

        auto deserialized = serializer.deserialize(serialized);
        EXPECT_TRUE(areEqual(original, deserialized)) << "Deserialized structure mismatch";
    }
};

// Group: Basic Types (Atoms & Terms)
TEST_F(ExpressionSerializerTest, BooleanTrue) {
    verifyRoundTrip(True(), "True");
}

TEST_F(ExpressionSerializerTest, BooleanFalse) {
    verifyRoundTrip(False(), "False");
}

TEST_F(ExpressionSerializerTest, Variable) {
    verifyRoundTrip(Var("x"), "Var[x]");
}

TEST_F(ExpressionSerializerTest, FunctionConst) {
    verifyRoundTrip(Func("c"), "Func[c]()");
}

TEST_F(ExpressionSerializerTest, FunctionNary) {
    verifyRoundTrip(Func("f", { Var("x"), Var("y") }), "Func[f](Var[x], Var[y])");
}

TEST_F(ExpressionSerializerTest, Predicate) {
    verifyRoundTrip(Pred("P", { Var("x") }), "Pred[P](Var[x])");
}

TEST_F(ExpressionSerializerTest, Equality) {
    verifyRoundTrip(Equal(Var("a"), Var("b")), "Equal(Var[a], Var[b])");
}

// Group: Logical Operators
TEST_F(ExpressionSerializerTest, OpNot) {
    verifyRoundTrip(Not(True()), "Not(True)");
}

TEST_F(ExpressionSerializerTest, OpAnd) {
    verifyRoundTrip(And(True(), False()), "And(True, False)");
}

TEST_F(ExpressionSerializerTest, OpOr) {
    verifyRoundTrip(Or(True(), False()), "Or(True, False)");
}

TEST_F(ExpressionSerializerTest, OpImp) {
    verifyRoundTrip(Imp(True(), False()), "Imp(True, False)");
}

TEST_F(ExpressionSerializerTest, OpEqv) {
    verifyRoundTrip(Eqv(True(), False()), "Eqv(True, False)");
}

TEST_F(ExpressionSerializerTest, OpXor) {
    verifyRoundTrip(Xor(True(), False()), "Xor(True, False)");
}

// Group: Junctions
TEST_F(ExpressionSerializerTest, JunctionConjunction) {
    verifyRoundTrip(
        Conjunction({ True(), False(), True() }),
        "Conjunction(True, False, True)"
    );
}

TEST_F(ExpressionSerializerTest, JunctionDisjunction) {
    verifyRoundTrip(
        Disjunction({ Pred("A"), Pred("B") }),
        "Disjunction(Pred[A](), Pred[B]())"
    );
}

// Group: Quantifiers
TEST_F(ExpressionSerializerTest, QuantifierForall) {
    verifyRoundTrip(
        Forall(Var("x"), Pred("P", { Var("x") })),
        "Forall[x](Pred[P](Var[x]))"
    );
}

TEST_F(ExpressionSerializerTest, QuantifierExists) {
    verifyRoundTrip(
        Exists(Var("y"), Pred("Q", { Var("y") })),
        "Exists[y](Pred[Q](Var[y]))"
    );
}

// Group: Complex Structures & Formatting
TEST_F(ExpressionSerializerTest, ComplexNestedStructure) {
    // Exists[x]( And( P(x), Not(Q(x)) ) )
    auto expr = Exists(
        Var("x"),
        And(
            Pred("P", { Var("x") }),
            Not(Pred("Q", { Var("x") }))
        )
    );
    verifyRoundTrip(expr, "Exists[x](And(Pred[P](Var[x]), Not(Pred[Q](Var[x]))))");
}

// Complex Test Cases
TEST_F(ExpressionSerializerTest, Complex_KitchenSink) {
    auto expr = Forall(
        Var("x"),
        Imp(
            And(
                Pred("Human", { Var("x") }),
                Not(Pred("Mortal", { Var("x") }))
            ),
            Exists(
                Var("y"),
                And(
                    Equal(Func("mother_of", { Var("x") }), Var("y")),
                    Disjunction({
                        Pred("Love", { Var("y"), Var("x") }),
                        Pred("Hate", { Var("y"), Var("x") })
                        })
                )
            )
        )
    );
    verifyRoundTrip(expr, "Forall[x](Imp(And(Pred[Human](Var[x]), Not(Pred[Mortal](Var[x]))), "
        "Exists[y](And(Equal(Func[mother_of](Var[x]), Var[y]), "
        "Disjunction(Pred[Love](Var[y], Var[x]), Pred[Hate](Var[y], Var[x]))))))");
}

TEST_F(ExpressionSerializerTest, Complex_SymbolTorture) {
    auto expr = Pred(
        "NastyPred",
        {
            Var("Array[i]"),
            Var("C:\\Path\\"),
            Var("Func(a,b)"),
            Var("Space Name"),
            Var("Good[Boy]")
        }
    );
    verifyRoundTrip(expr, "Pred[NastyPred](Var[Array[i\\]], Var[C:\\\\Path\\\\], Var[Func(a,b)], Var[Space Name], Var[Good[Boy\\]])");
}

TEST_F(ExpressionSerializerTest, Complex_DeepRecursion) {
    TermPtr current = Var("x");
    for (int i = 0; i < 50; ++i) {
        current = Func("f", { current });
    }
    auto expr = Equal(current, Var("y"));

    std::string s = serializer.serialize(expr);
    auto deserialized = serializer.deserialize(s);
    EXPECT_TRUE(areEqual(expr, deserialized));
}

TEST_F(ExpressionSerializerTest, Complex_EmptyStructures) {
    auto expr = Conjunction({
        Pred("IsAtom"),
        Equal(Func("EmptyFunc"), Var("EmptyVar")),
        Disjunction({})
        });
    verifyRoundTrip(expr, "Conjunction(Pred[IsAtom](), Equal(Func[EmptyFunc](), Var[EmptyVar]), Disjunction())");
}

TEST_F(ExpressionSerializerTest, Complex_WhitespaceHell) {
    std::string hell = "\n\tAnd( \n\t\tPred[ P ]( Var[ x ] ) ,\r\n\t\tTrue\n   )   \n";
    auto expectedExpr = And(Pred(" P ", { Var(" x ") }), True());
    auto deserialized = serializer.deserialize(hell);
    EXPECT_TRUE(areEqual(expectedExpr, deserialized));
}

TEST_F(ExpressionSerializerTest, WhitespaceTolerance) {
    // Whitespace allowed only inside argument lists
    std::string messyInput = "And(  Pred[x]() ,   True  )";
    auto expr = serializer.deserialize(messyInput);

    std::string canonical = "And(Pred[x](), True)";
    EXPECT_EQ(serializer.serialize(expr), canonical);
}

TEST_F(ExpressionSerializerTest, TheNuclearOption) {
    std::string expectedDisj = "Disjunction(";
    std::vector<FormulaPtr> disjArgs;

    for (int i = 0; i < 50; ++i) {
        if (i % 3 == 0) {
            disjArgs.push_back(True());
            expectedDisj += "True";
        }
        else if (i % 3 == 1) {
            disjArgs.push_back(FormulaPtr());
            expectedDisj += "Null";
        }
        else {
            std::string name = "P_" + std::to_string(i);
            disjArgs.push_back(Pred(name));
            expectedDisj += "Pred[" + name + "]()";
        }
        if (i < 49) expectedDisj += ", ";
    }
    expectedDisj += ")";

    FormulaPtr deepStructure = False();
    std::string expectedDeep = "False";
    for (int i = 0; i < 20; ++i) {
        deepStructure = Not(deepStructure);
        expectedDeep = "Not(" + expectedDeep + ")";
    }

    auto termFromHell = Func(
        "Start[\n]\tEnd",
        {
            Var(""),
            Var(" "),
            Var("a,b"),
            Var("x(y)"),
            Var("Null"),
            Var("["),
            Var("]"),
            Var("\\")
        }
    );

    std::string expectedTerm = "Func[Start[\n\\]\tEnd]("
        "Var[], "
        "Var[ ], "
        "Var[a,b], "
        "Var[x(y)], "
        "Var[Null], "
        "Var[[], "
        "Var[\\]], "
        "Var[\\\\]"
        ")";

    auto expr = Xor(
        Imp(
            Disjunction(disjArgs),
            Eqv(deepStructure, Pred("Mid"))
        ),
        Forall(
            Var("GlobalVar"),
            Exists(
                Var("LocalVar"),
                Equal(termFromHell, Var("Result"))
            )
        )
    );

    std::string expected =
        "Xor("
        "Imp(" + expectedDisj + ", Eqv(" + expectedDeep + ", Pred[Mid]())), "
        "Forall[GlobalVar]("
        "Exists[LocalVar]("
        "Equal(" + expectedTerm + ", Var[Result])"
        ")"
        ")"
        ")";

    verifyRoundTrip(expr, expected);
}

// Group: Escaping
TEST_F(ExpressionSerializerTest, EscapingBrackets) {
    // Symbol with ']' becomes '\]'
    auto var = Var("arr[i]");
    verifyRoundTrip(var, "Var[arr[i\\]]");
}

TEST_F(ExpressionSerializerTest, EscapingBackslash) {
    // Symbol with '\' becomes '\\'
    auto var = Var("path\\file");
    verifyRoundTrip(var, "Var[path\\\\file]");
}

// Group: Sequences
TEST_F(ExpressionSerializerTest, SequenceRoundTrip) {
    std::vector<ExpressionPtr> list = { Var("x"), True() };

    std::string serialized = serializer.serializeSequence(list);
    EXPECT_EQ(serialized, "Var[x]\nTrue\n");

    auto deserialized = serializer.deserializeSequence(serialized);
    ASSERT_EQ(deserialized.size(), 2);
    EXPECT_TRUE(areEqual(deserialized[0], list[0]));
    EXPECT_TRUE(areEqual(deserialized[1], list[1]));
}

// Group: Negative Tests
TEST_F(ExpressionSerializerTest, ErrorMissingKeyword) {
    EXPECT_THROW(serializer.deserialize("[x]"), ExpressionSerializer::ParserException);
}

TEST_F(ExpressionSerializerTest, ErrorSpaceBetweenKeywordAndArgs) {
    // Space forbidden before parenthesis
    EXPECT_THROW(serializer.deserialize("And (True, False)"), ExpressionSerializer::ParserException);
}

TEST_F(ExpressionSerializerTest, ErrorTypeMismatch) {
    // Not expects Formula, Var is Term
    EXPECT_THROW(serializer.deserialize("Not(Var[x])"), ExpressionSerializer::ParserException);
}

TEST_F(ExpressionSerializerTest, ErrorGarbageAtEnd) {
    EXPECT_THROW(serializer.deserialize("True garbage"), ExpressionSerializer::ParserException);
}

// Group: Distinct Objects
TEST_F(ExpressionSerializerTest, DistinctObject) {
    // Distinct objects serialize without parentheses
    verifyRoundTrip(Distinct("Apple"), "Distinct[Apple]");
}

TEST_F(ExpressionSerializerTest, DistinctObjectNumeric) {
    verifyRoundTrip(Distinct("123"), "Distinct[123]");
}

TEST_F(ExpressionSerializerTest, DistinctVsFunction) {
    // Ensure Distinct[c] and Func[c]() are treated differently
    auto d = Distinct("c");
    auto f = Func("c");

    // 1. Check Serialization format
    EXPECT_EQ(serializer.serialize(d), "Distinct[c]");
    EXPECT_EQ(serializer.serialize(f), "Func[c]()");

    // 2. Check Round-Trip types
    auto dBack = serializer.deserialize(serializer.serialize(d));
    auto fBack = serializer.deserialize(serializer.serialize(f));

    EXPECT_TRUE(areEqual(d, dBack));
    EXPECT_TRUE(areEqual(f, fBack));

    // 3. Verify internal flag correctness
    auto dTerm = std::static_pointer_cast<FunctionTerm>(dBack);
    auto fTerm = std::static_pointer_cast<FunctionTerm>(fBack);
    EXPECT_TRUE(dTerm->distinct);
    EXPECT_FALSE(fTerm->distinct);
}

TEST_F(ExpressionSerializerTest, DistinctNested) {
    // Predicate containing a distinct object and a variable
    auto expr = Pred("HasValue", { Var("x"), Distinct("42") });
    verifyRoundTrip(expr, "Pred[HasValue](Var[x], Distinct[42])");
}

TEST_F(ExpressionSerializerTest, DistinctEscaping) {
    verifyRoundTrip(Distinct("Obj[1]"), "Distinct[Obj[1\\]]");
}

// Group: Negative Tests (Distinct Objects)
TEST_F(ExpressionSerializerTest, ErrorDistinctWithArguments) {
    // Distinct objects are atomic and cannot have arguments
    // The parser stops after Distinct[Obj], so (Var[x]) remains unparsed
    EXPECT_THROW(serializer.deserialize("Distinct[Obj](Var[x])"), ExpressionSerializer::ParserException);
}

TEST_F(ExpressionSerializerTest, ErrorDistinctWithEmptyParens) {
    // Distinct objects do not use parentheses at all
    // '()' after Distinct[Obj] should be treated as unexpected characters
    EXPECT_THROW(serializer.deserialize("Distinct[Obj]()"), ExpressionSerializer::ParserException);
}

#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../Expression.hpp"
#include "../TptpTool/Loader.hpp"

namespace fs = std::filesystem;
using namespace TptpTool;

class LoaderTest : public ::testing::Test {
protected:
    std::string tptpDir;
    std::vector<std::string> tempFiles;
    std::vector<std::string> tempDirs;

    void SetUp() override {
        const char* envName = "TPTP_DIR";
        const char* envVal = std::getenv(envName);
        // Fallback to current dir if TPTP_DIR not set (for local temp file tests)
        tptpDir = envVal ? envVal : fs::current_path().string();
    }

    void TearDown() override {
        // Clean up files
        for (const auto& file : tempFiles) {
            if (fs::exists(file)) fs::remove(file);
        }
        // Clean up directories (reverse order to delete subdirs first)
        for (auto it = tempDirs.rbegin(); it != tempDirs.rend(); ++it) {
            if (fs::exists(*it)) fs::remove(*it);
        }
    }

    // Creates file and registers for cleanup
    std::string createTempFile(const std::string& filename, const std::string& content) {
        // Ensure parent path exists if filename contains directories
        fs::path p(filename);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
            tempDirs.push_back(p.parent_path().string());
        }

        std::ofstream tmp(filename);
        if (!tmp.is_open()) {
            throw std::runtime_error("Test Setup Failed: Could not create file " + filename);
        }
        tmp << content;
        tmp.close();
        tempFiles.push_back(filename);
        return filename;
    }

    // --- Helpers ---

    Loader::AnnotatedFormula getFirstAnnotated(const std::string& tptpText) {
        Loader loader(tptpDir);
        auto result = loader.loadFromText(tptpText);
        if (result.formulas.empty()) {
            throw std::runtime_error("Test Helper Error: No formulas parsed.");
        }
        return result.formulas[0];
    }

    FormulaPtr getFormula(const std::string& tptpText) {
        return getFirstAnnotated(tptpText).formula;
    }

    template <typename T>
    std::shared_ptr<T> as(ExpressionPtr expr) {
        auto casted = std::dynamic_pointer_cast<T>(expr);
        EXPECT_NE(casted, nullptr) << "CRITICAL: Wrong AST type encountered.";
        return casted;
    }

    void checkPred(FormulaPtr f, const std::string& sym, size_t arity) {
        auto p = as<PredicateFormula>(f);
        if (p) {
            EXPECT_EQ(p->symbol, sym);
            EXPECT_EQ(p->arguments.size(), arity);
        }
    }
};

// =========================================================================
// GROUP 1: METADATA & ROLES
// =========================================================================

TEST_F(LoaderTest, Meta_CheckRoles) {
    std::vector<std::string> roles = {
        "axiom", "hypothesis", "definition", "assumption",
        "lemma", "theorem", "conjecture", "negated_conjecture", "plain"
    };

    for (const auto& role : roles) {
        std::string content = "fof(test_name, " + role + ", p).";
        auto af = getFirstAnnotated(content);
        EXPECT_EQ(af.role, role);
        EXPECT_EQ(af.name, "test_name");
    }
}

TEST_F(LoaderTest, Meta_CheckNames_Complex) {
    // Case 1: Integer name
    // In TPTP, formula names can be integers. We store them as string identifiers.
    auto af1 = getFirstAnnotated("fof(123, axiom, p).");
    EXPECT_EQ(af1.name, "123");

    // Case 2: Single-quoted name
    // The parser MUST strip the quotes for the internal name representation.
    auto af2 = getFirstAnnotated("fof('complex_name', axiom, p).");
    EXPECT_EQ(af2.name, "complex_name");
}

// =========================================================================
// GROUP 2: STRICT NORMALIZATION (The "Quotes" Bug)
// =========================================================================

TEST_F(LoaderTest, Norm_StripQuotes_Simple) {
    auto f = getFormula("fof(t, axiom, p('a', a)).");
    auto p = as<PredicateFormula>(f);
    ASSERT_NE(p, nullptr);

    auto t1 = as<FunctionTerm>(p->arguments[0]); // 'a'
    auto t2 = as<FunctionTerm>(p->arguments[1]); // a

    // Check 1: Strip quotes from 'a'
    EXPECT_EQ(t1->symbol, "a") << "FAIL: Parser must strip quotes from 'a'";

    // Check 2: Standard constants are NOT distinct
    EXPECT_FALSE(t1->distinct) << "Standard constant 'a' should NOT be distinct";
    EXPECT_FALSE(t2->distinct) << "Standard constant a should NOT be distinct";

    EXPECT_EQ(t1->symbol, t2->symbol) << "FAIL: 'a' and a must be identical symbols";
}

TEST_F(LoaderTest, Norm_StripQuotes_SnakeCase) {
    auto f = getFormula("fof(t, axiom, p('my_func')).");
    auto p = as<PredicateFormula>(f);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(as<FunctionTerm>(p->arguments[0])->symbol, "my_func");
}

TEST_F(LoaderTest, Norm_StripQuotes_FromSpecialConsts) {
    // 'Big' starts with uppercase, looking like a Variable.
    // However, single quotes make it a Constant (FunctionTerm).
    // The parser MUST strip quotes but preserve it as a FunctionTerm.

    auto f = getFormula("fof(t, axiom, p('Big', 'Space X')).");
    auto p = as<PredicateFormula>(f);
    ASSERT_NE(p, nullptr);

    auto t1 = p->arguments[0]; // 'Big'
    auto t2 = p->arguments[1]; // 'Space X'

    // Check 1: They must be FunctionTerms (Constants), NOT Variables.
    auto ft1 = as<FunctionTerm>(t1);
    auto ft2 = as<FunctionTerm>(t2);

    EXPECT_TRUE(ft1 != nullptr) << "Upper-case in quotes must remain a Constant (FunctionTerm)";
    EXPECT_TRUE(ft2 != nullptr);

    // Check 2: Symbols must be stripped of quotes.
    EXPECT_EQ(ft1->symbol, "Big");
    EXPECT_EQ(ft2->symbol, "Space X");

    // Check 3: These are standard constants, not distinct objects
    EXPECT_FALSE(ft1->distinct);
    EXPECT_FALSE(ft2->distinct);
}

// =========================================================================
// GROUP 3: TERMS & TYPES
// =========================================================================

TEST_F(LoaderTest, Strict_Type_Numbers) {
    // Goal: Numbers (123) are FunctionTerms with distinct=true
    std::string content = "fof(num_check, axiom, p(123)).";

    auto f = getFormula(content);
    auto p = as<PredicateFormula>(f);
    ASSERT_NE(p, nullptr);

    auto arg = as<FunctionTerm>(p->arguments[0]);

    // 1. Symbol should be just the number string
    EXPECT_EQ(arg->symbol, "123");

    // 2. MUST be marked as distinct
    EXPECT_TRUE(arg->distinct) << "Number 123 must have distinct=true";
}

TEST_F(LoaderTest, Strict_Type_DistinctObject) {
    // Goal: "Apple" is FunctionTerm with distinct=true and quotes in symbol
    std::string content = "fof(t, axiom, p(\"Apple\")).";

    auto f = getFormula(content);
    auto p = as<PredicateFormula>(f);
    ASSERT_NE(p, nullptr);

    auto arg = as<FunctionTerm>(p->arguments[0]);

    // 1. Symbol should contain the quotes: "Apple"
    EXPECT_EQ(arg->symbol, "\"Apple\"");

    // 2. MUST be marked as distinct
    EXPECT_TRUE(arg->distinct) << "Distinct object \"Apple\" must have distinct=true";
}

TEST_F(LoaderTest, Type_System_TrueFalse) {
    auto f1 = getFormula("fof(t, axiom, $true).");
    EXPECT_TRUE(as<BooleanFormula>(f1)->value);

    auto f2 = getFormula("fof(f, axiom, $false).");
    EXPECT_FALSE(as<BooleanFormula>(f2)->value);
}

TEST_F(LoaderTest, Type_Variables) {
    auto f = getFormula("fof(t, axiom, ![X]: p(X)).");
    auto q = as<QuantificationFormula>(f);
    EXPECT_EQ(q->variable->symbol, "X");

    auto p = as<PredicateFormula>(q->body);
    auto v = as<VariableTerm>(p->arguments[0]);
    EXPECT_EQ(v->symbol, "X");
}

// =========================================================================
// GROUP 4: OPERATOR DESUGARING
// =========================================================================

TEST_F(LoaderTest, Op_XOR) {
    auto f = getFormula("fof(x, axiom, p <~> q).");
    EXPECT_EQ(as<BinaryFormula>(f)->op, BinaryFormula::Operator::XOR);
}

TEST_F(LoaderTest, Op_ReverseImp) {
    // p <= q  =>  q => p
    auto f = getFormula("fof(x, axiom, p <= q).");
    auto bin = as<BinaryFormula>(f);
    EXPECT_EQ(bin->op, BinaryFormula::Operator::IMP);
    checkPred(bin->left, "q", 0);
    checkPred(bin->right, "p", 0);
}

TEST_F(LoaderTest, Op_NAND_NOR) {
    // p ~& q => ~(p & q)
    auto fNand = getFormula("fof(x, axiom, p ~& q).");
    auto negN = as<NegationFormula>(fNand);
    ASSERT_NE(negN, nullptr);
    // Check if inner is AND (Structure might differ slightly but must be AND logic)
    bool isAnd = false;
    if (auto b = std::dynamic_pointer_cast<BinaryFormula>(negN->child)) isAnd = (b->op == BinaryFormula::Operator::AND);
    else if (auto j = std::dynamic_pointer_cast<JunctionFormula>(negN->child)) isAnd = (j->op == JunctionFormula::Operator::AND);
    EXPECT_TRUE(isAnd) << "NAND must be ~(... & ...)";

    // p ~| q => ~(p | q)
    auto fNor = getFormula("fof(x, axiom, p ~| q).");
    auto negO = as<NegationFormula>(fNor);
    ASSERT_NE(negO, nullptr);
    bool isOr = false;
    if (auto b = std::dynamic_pointer_cast<BinaryFormula>(negO->child)) isOr = (b->op == BinaryFormula::Operator::OR);
    else if (auto j = std::dynamic_pointer_cast<JunctionFormula>(negO->child)) isOr = (j->op == JunctionFormula::Operator::OR);
    EXPECT_TRUE(isOr) << "NOR must be ~(... | ...)";
}

// =========================================================================
// GROUP 5: CNF STRUCTURE
// =========================================================================

TEST_F(LoaderTest, CNF_Structure) {
    auto f = getFormula("cnf(clause1, axiom, p | ~q | r).");
    bool isOr = false;
    if (auto b = std::dynamic_pointer_cast<BinaryFormula>(f)) isOr = (b->op == BinaryFormula::Operator::OR);
    else if (auto j = std::dynamic_pointer_cast<JunctionFormula>(f)) isOr = (j->op == JunctionFormula::Operator::OR);
    EXPECT_TRUE(isOr) << "CNF body must be OR";
}

// =========================================================================
// GROUP 6: ERRORS & SYNTAX
// =========================================================================

TEST_F(LoaderTest, Error_StrictSyntax) {
    Loader loader;
    EXPECT_THROW(loader.loadFromText("fof(a, axiom, p)"), std::runtime_error); // No dot
    EXPECT_THROW(loader.loadFromText("fof(a, axiom, (p)."), std::runtime_error); // Parens
    EXPECT_THROW(loader.loadFromText("nonsense"), std::runtime_error); // Garbage
}

// =========================================================================
// GROUP 7: RECURSION, INCLUDES & FILE SYSTEM
// =========================================================================

TEST_F(LoaderTest, Rec_SimpleInclude) {
    // A includes B. Result should have formulas from both.
    std::string fileB = createTempFile("B.p", "fof(b_ax, axiom, b).");
    std::string fileA = createTempFile("A.p", "include('B.p'). fof(a_ax, axiom, a).");

    Loader loader(tptpDir); // Use current dir context
    auto result = loader.loadRecursively(fileA);

    EXPECT_EQ(result.size(), 2);
    bool foundA = false, foundB = false;
    for (const auto& f : result) {
        if (f.name == "a_ax") foundA = true;
        if (f.name == "b_ax") foundB = true;
    }
    EXPECT_TRUE(foundA);
    EXPECT_TRUE(foundB);
}

TEST_F(LoaderTest, Rec_NestedDepth) {
    // A -> B -> C
    std::string fileC = createTempFile("C.p", "fof(c, axiom, c).");
    std::string fileB = createTempFile("B.p", "include('C.p'). fof(b, axiom, b).");
    std::string fileA = createTempFile("A.p", "include('B.p'). fof(a, axiom, a).");

    Loader loader(tptpDir);
    auto result = loader.loadRecursively(fileA);

    EXPECT_EQ(result.size(), 3);
}

TEST_F(LoaderTest, Rec_DiamondDependency) {
    // A includes B and C. Both B and C include D.
    std::string fileD = createTempFile("D.p", "fof(d, axiom, d).");
    std::string fileC = createTempFile("C.p", "include('D.p'). fof(c, axiom, c).");
    std::string fileB = createTempFile("B.p", "include('D.p'). fof(b, axiom, b).");
    std::string fileA = createTempFile("A.p", "include('B.p'). include('C.p'). fof(a, axiom, a).");

    Loader loader(tptpDir);
    auto result = loader.loadRecursively(fileA);

    EXPECT_GE(result.size(), 4); // a, b, c, d (d might appear twice)

    // Check if D is present
    bool foundD = false;
    for (const auto& f : result) if (f.name == "d") foundD = true;
    EXPECT_TRUE(foundD);
}

TEST_F(LoaderTest, Rec_Cycle_Safety) {
    // A includes B, B includes A.
    // This MUST NOT cause an infinite loop / stack overflow.
    std::string fileB_Name = "cycle_B.p"; // Use names to allow cross-ref
    std::string fileA_Name = "cycle_A.p";

    createTempFile(fileB_Name, "include('cycle_A.p'). fof(b, axiom, b).");
    createTempFile(fileA_Name, "include('cycle_B.p'). fof(a, axiom, a).");

    Loader loader(tptpDir);

    try {
        auto result = loader.loadRecursively(fileA_Name);
        EXPECT_GE(result.size(), 2);
    }
    catch (const std::runtime_error&) {
        SUCCEED();
    }
    catch (...) {
        FAIL() << "Unknown exception during cycle test.";
    }
}

TEST_F(LoaderTest, Rec_IncludeWithSelection) {
    // TPTP syntax: include('file', [name1, name2]).
    std::string sub = createTempFile("sub_sel.p", "fof(x, axiom, x). fof(y, axiom, y).");
    std::string main = createTempFile("main_sel.p", "include('sub_sel.p', [x]).");

    Loader loader(tptpDir);
    EXPECT_NO_THROW({
        auto result = loader.loadRecursively(main);
        EXPECT_GE(result.size(), 1);
        });
}

TEST_F(LoaderTest, Rec_MissingFile) {
    // Include a file that doesn't exist. Should throw.
    std::string main = createTempFile("missing.p", "include('ghost.p').");
    Loader loader(tptpDir);
    EXPECT_THROW(loader.loadRecursively(main), std::runtime_error);
}

TEST_F(LoaderTest, Rec_RelativePaths) {
    // Test subdirectory handling: dir/A.p includes ../B.p
    fs::create_directories("subdir");
    tempDirs.push_back("subdir"); // Mark for cleanup

    std::string fileB = createTempFile("B_rel.p", "fof(b, axiom, b).");
    std::string fileA = createTempFile("subdir/A_rel.p", "include('../B_rel.p'). fof(a, axiom, a).");

    Loader loader(tptpDir);
    auto result = loader.loadRecursively(fileA);

    bool foundB = false;
    for (const auto& f : result) if (f.name == "b") foundB = true;
    EXPECT_TRUE(foundB) << "Failed to resolve relative path '../B_rel.p'";
}

// =========================================================================
// GROUP 8: SYNTAX TORTURE & ESCAPING
// =========================================================================

TEST_F(LoaderTest, Syntax_EscapedQuotes_InAtom) {
    // TPTP allows escaping quotes inside quoted atoms: 'It\'s raining'
    auto f = getFormula("fof(esc, axiom, p('It\\'s raining')).");
    auto p = as<PredicateFormula>(f);
    ASSERT_NE(p, nullptr);
    auto t = as<FunctionTerm>(p->arguments[0]);

    // Regular atom, not distinct
    EXPECT_FALSE(t->distinct);
    EXPECT_TRUE(t->symbol.find("'") != std::string::npos);
    EXPECT_TRUE(t->symbol.find("raining") != std::string::npos);
}

TEST_F(LoaderTest, Syntax_EscapedQuotes_InDistinctObject) {
    // Distinct object with escaped quote: "He said \"Hello\""
    // Expect symbol to retain quotes, e.g., "He said \"Hello\""
    auto f = getFormula("fof(esc, axiom, p(\"He said \\\"Hello\\\"\")).");
    auto p = as<PredicateFormula>(f);
    auto t = as<FunctionTerm>(p->arguments[0]);

    EXPECT_TRUE(t->distinct);
    // Ensure the string didn't end prematurely
    EXPECT_TRUE(t->symbol.find("Hello") != std::string::npos);
    EXPECT_TRUE(t->symbol.find("\"") != std::string::npos) << "Symbol should retain outer quotes";
}

TEST_F(LoaderTest, Syntax_TortureLayout) {
    // TPTP allows whitespace and comments ANYWHERE between tokens.
    std::string content = R"(
        fof(  torture , 
           axiom , 
           p  (  % Comment inside args
              a , 
              /* Block comment 
                  inside args */ b
            ) 
            => 
            q
        ).
    )";
    auto f = getFormula(content);
    auto bin = as<BinaryFormula>(f);
    EXPECT_EQ(bin->op, BinaryFormula::Operator::IMP);
    checkPred(bin->right, "q", 0);

    auto left = as<PredicateFormula>(bin->left);
    EXPECT_EQ(left->symbol, "p");
    EXPECT_EQ(left->arguments.size(), 2);
}

TEST_F(LoaderTest, Syntax_CNF_Parentheses) {
    // cnf(name, role, ( p | q | r )).
    auto f = getFormula("cnf(brackets, axiom, ( p | ~q )).");

    bool isOr = false;
    if (auto b = std::dynamic_pointer_cast<BinaryFormula>(f)) isOr = (b->op == BinaryFormula::Operator::OR);
    else if (auto j = std::dynamic_pointer_cast<JunctionFormula>(f)) isOr = (j->op == JunctionFormula::Operator::OR);

    EXPECT_TRUE(isOr) << "Failed to parse CNF wrapped in parentheses";
}

TEST_F(LoaderTest, Syntax_Equality_Infix_Mix) {
    // ![X]: (f(X) = g(X) => X != a)
    auto f = getFormula("fof(eq_mix, axiom, ![X]: ( f(X) = g(X) => X != a )).");

    auto q = as<QuantificationFormula>(f);
    auto body = as<BinaryFormula>(q->body); // IMP

    auto eqLeft = as<EqualityFormula>(body->left);
    ASSERT_NE(eqLeft, nullptr) << "Left side of => should be Equality";

    auto negRight = as<NegationFormula>(body->right);
    ASSERT_NE(negRight, nullptr) << "Right side '!=' should be Negation";
    auto eqRight = as<EqualityFormula>(negRight->child);
    ASSERT_NE(eqRight, nullptr) << "Inside '!=' should be Equality";
}

// =========================================================================
// GROUP 9: STRESS TESTS
// =========================================================================

TEST_F(LoaderTest, Stress_DeepNesting) {
    // 500 levels deep.
    int depth = 500;
    std::string term = "a";
    for (int i = 0; i < depth; ++i) {
        term = "f(" + term + ")";
    }
    std::string content = "fof(deep, axiom, p(" + term + ")).";

    auto f = getFormula(content);
    auto p = as<PredicateFormula>(f);
    ASSERT_NE(p, nullptr);

    auto current = p->arguments[0];
    for (int i = 0; i < 10; ++i) {
        auto ft = as<FunctionTerm>(current);
        ASSERT_NE(ft, nullptr);
        EXPECT_EQ(ft->symbol, "f");
        current = ft->arguments[0];
    }
}

TEST_F(LoaderTest, Stress_MassiveArity) {
    // Predicate with 1000 arguments: p(a, a, a, ...)
    std::string args;
    for (int i = 0; i < 1000; ++i) {
        args += "a";
        if (i < 999) args += ",";
    }
    std::string content = "fof(wide, axiom, p(" + args + ")).";

    auto f = getFormula(content);
    auto p = as<PredicateFormula>(f);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->arguments.size(), 1000);
}

// =========================================================================
// GROUP 10: STRICT SEMANTICS & MISSING FEATURES CHECK
// =========================================================================

TEST_F(LoaderTest, Strict_IteF_MustDesugarOrHaveType) {
    // $ite_f(Cond, Then, Else) -> IF Cond THEN Then ELSE Else
    std::string content = "fof(ite_logic, axiom, $ite_f(p, q, r)).";

    try {
        auto f = getFormula(content);
        ASSERT_NE(f, nullptr);

        if (auto p = std::dynamic_pointer_cast<PredicateFormula>(f)) {
            FAIL() << "CRITICAL: Parser loaded $ite_f as a raw PredicateFormula. ";
        }

        bool isAnd = false;
        if (auto b = std::dynamic_pointer_cast<BinaryFormula>(f)) isAnd = (b->op == BinaryFormula::Operator::AND);
        else if (auto j = std::dynamic_pointer_cast<JunctionFormula>(f)) isAnd = (j->op == JunctionFormula::Operator::AND);

        EXPECT_TRUE(isAnd) << "FAIL: $ite_f structure invalid. Expected logic expansion (AND) or specialized ITE type.";

    }
    catch (const std::exception& e) {
        FAIL() << "Parser crashed on $ite_f. Implement desugaring! Error: " << e.what();
    }
}

TEST_F(LoaderTest, Strict_Distinct_MustDesugar) {
    // $distinct(a, b) -> a != b -> ~(a = b)
    std::string content = "fof(distinct_logic, axiom, $distinct(a, b)).";

    try {
        auto f = getFormula(content);
        ASSERT_NE(f, nullptr);

        if (auto p = std::dynamic_pointer_cast<PredicateFormula>(f)) {
            FAIL() << "CRITICAL: Parser loaded $distinct as raw PredicateFormula. ";
        }

        auto neg = as<NegationFormula>(f);
        if (!neg) {
            FAIL() << "FAIL: Expected NegationFormula (as part of a != b), got something else.";
        }
        auto eq = std::dynamic_pointer_cast<EqualityFormula>(neg->child);
        if (!eq) {
            FAIL() << "FAIL: Expected EqualityFormula inside Negation for $distinct.";
        }
    }
    catch (const std::exception& e) {
        FAIL() << "Parser crashed on $distinct. Implement desugaring! Error: " << e.what();
    }
}

TEST_F(LoaderTest, Strict_Distinct_4Args_MustGenerateAllPairs) {
    // $distinct(a, b, c, d) implies 6 inequalities:
    // a!=b, a!=c, a!=d, b!=c, b!=d, c!=d
    std::string content = "fof(distinct_4, axiom, $distinct(a, b, c, d)).";

    try {
        auto f = getFormula(content);
        ASSERT_NE(f, nullptr);

        // Store found pairs for verification
        std::vector<std::pair<std::string, std::string>> foundPairs;

        // Recursive helper to traverse the AND-tree
        std::function<void(FormulaPtr)> traverse = [&](FormulaPtr curr) {
            // 1. If Binary AND, traverse both children
            if (auto bin = std::dynamic_pointer_cast<BinaryFormula>(curr)) {
                if (bin->op == BinaryFormula::Operator::AND) {
                    traverse(bin->left);
                    traverse(bin->right);
                    return;
                }
            }

            // 2. If Negation(Equality), extract terms
            if (auto neg = std::dynamic_pointer_cast<NegationFormula>(curr)) {
                if (auto eq = std::dynamic_pointer_cast<EqualityFormula>(neg->child)) {
                    auto funcL = std::dynamic_pointer_cast<FunctionTerm>(eq->left);
                    auto funcR = std::dynamic_pointer_cast<FunctionTerm>(eq->right);

                    ASSERT_TRUE(funcL && funcR) << "Terms in equality must be functions/constants";
                    foundPairs.push_back({ funcL->symbol, funcR->symbol });
                    return;
                }
            }

            FAIL() << "Unexpected structure in $distinct expansion. Expected AND node or Negation(Equality).";
            };

        traverse(f);

        // 4 items choose 2 = 6 pairs
        ASSERT_EQ(foundPairs.size(), 6) << "Should generate exactly 6 inequalities for 4 distinct arguments";

        // Expected pairs based on parser logic (i=0..n, j=i+1..n)
        std::vector<std::pair<std::string, std::string>> expected = {
            {"a", "b"}, {"a", "c"}, {"a", "d"},
            {"b", "c"}, {"b", "d"},
            {"c", "d"}
        };

        // Verify that every expected pair exists in the parsed formula
        for (const auto& expect : expected) {
            bool found = false;
            for (const auto& fp : foundPairs) {
                if (fp.first == expect.first && fp.second == expect.second) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Missing inequality: " << expect.first << " != " << expect.second;
        }

    }
    catch (const std::exception& e) {
        FAIL() << "Parser crashed on $distinct(4 args). Error: " << e.what();
    }
}

TEST_F(LoaderTest, Strict_Numbers_NeedType) {
    // Numbers (123) are now FunctionTerms with distinct=true
    std::string content = "fof(num_check, axiom, p(123)).";
    auto f = getFormula(content);
    auto p = as<PredicateFormula>(f);
    ASSERT_NE(p, nullptr);
    ExpressionPtr arg = p->arguments[0];

    // --- CHECK 1: Is it a Term? ---
    EXPECT_TRUE(arg->isTerm()) << "123 must be a Term.";

    // --- CHECK 2: Is it strictly a Distinct FunctionTerm? ---
    auto ft = std::dynamic_pointer_cast<FunctionTerm>(arg);
    ASSERT_NE(ft, nullptr) << "123 should be loaded as FunctionTerm";

    EXPECT_EQ(ft->symbol, "123");
    EXPECT_TRUE(ft->distinct) << "Numbers must be distinct";
}

TEST_F(LoaderTest, Strict_Quote_Equivalence) {
    // Goal: Verify that 'a' is semantically identical to a (distinct=false),
    // whereas "a" (double quotes) is a Distinct Object (distinct=true).

    std::string content = "fof(eq_check, axiom, p(a, 'a', \"a\")).";

    auto f = getFormula(content);
    auto p = as<PredicateFormula>(f);

    auto plain = as<FunctionTerm>(p->arguments[0]);   // a
    auto singleQ = as<FunctionTerm>(p->arguments[1]); // 'a'
    auto distinct = as<FunctionTerm>(p->arguments[2]); // "a"

    // 1. 'a' must be identical to a (quotes stripped, distinct=false)
    EXPECT_EQ(plain->symbol, "a");
    EXPECT_FALSE(plain->distinct);

    EXPECT_EQ(singleQ->symbol, "a");
    EXPECT_FALSE(singleQ->distinct);

    EXPECT_EQ(plain->symbol, singleQ->symbol);

    // 2. "a" must be distinct=true and keep quotes in symbol
    EXPECT_TRUE(distinct->distinct);
    EXPECT_EQ(distinct->symbol, "\"a\"");

    // 3. Comparison
    EXPECT_NE(plain->symbol, distinct->symbol);
    EXPECT_NE(plain->distinct, distinct->distinct);
}

TEST_F(LoaderTest, Strict_Nested_Quotes_Distinct) {
    // Input: "'inner'" 
    // Interpretation: Distinct Object containing the string: 'inner'
    std::string content = "fof(nest, axiom, p(\"'inner'\")).";

    auto f = getFormula(content);
    auto p = as<PredicateFormula>(f);
    auto ft = as<FunctionTerm>(p->arguments[0]);

    // Check distinct flag
    EXPECT_TRUE(ft->distinct);

    // Check if the inner content preserves the single quotes and outer double quotes
    EXPECT_EQ(ft->symbol, "\"'inner'\"");
}

TEST_F(LoaderTest, Strict_Include_Filter) {
    // 1. Prepare axioms content
    std::string axContent =
        "fof(ax1, axiom, $true).\n"
        "fof(ax2, axiom, $true).\n"
        "fof(ax3, axiom, $true).\n";

    // 2. Prepare main file content with selective include
    std::string mainContent =
        "include('temp_axioms.ax', [ax1, ax3]).\n"
        "fof(goal, conjecture, $true).\n";

    // 3. Create files using fixture helper
    createTempFile("temp_axioms.ax", axContent);
    createTempFile("temp_main.p", mainContent);

    try {
        Loader loader(tptpDir);

        // Use the correct method from your Loader header
        std::vector<Loader::AnnotatedFormula> formulas = loader.loadRecursively("temp_main.p");

        // 4. Verify results
        bool foundAx1 = false;
        bool foundAx2 = false;
        bool foundAx3 = false;
        bool foundGoal = false;

        for (const auto& f : formulas) {
            if (f.name == "ax1") foundAx1 = true;
            if (f.name == "ax2") foundAx2 = true;
            if (f.name == "ax3") foundAx3 = true;
            if (f.name == "goal") foundGoal = true;
        }

        EXPECT_TRUE(foundAx1) << "Filter failed: ax1 should be present";
        EXPECT_TRUE(foundAx3) << "Filter failed: ax3 should be present";
        EXPECT_TRUE(foundGoal) << "Main file content (goal) should be present";

        // This is the key check for the filter feature
        EXPECT_FALSE(foundAx2) << "Filter failed: ax2 should have been filtered out via include list!";
    }
    catch (const std::exception& e) {
        FAIL() << "Loader crashed during include test: " << e.what();
    }
}

TEST_F(LoaderTest, Strict_Block_Comments) {
    std::string content =
        "/* Header Comment */\n"
        "fof(test_block, axiom, p( /* inline block */ a )).\n"
        "/* \n"
        "   Multi-line footer \n"
        "*/";

    try {
        auto f = getFormula(content);
        ASSERT_NE(f, nullptr);

        checkPred(f, "p", 1);

        auto pred = std::dynamic_pointer_cast<PredicateFormula>(f);
        auto arg = std::dynamic_pointer_cast<FunctionTerm>(pred->arguments[0]);
        EXPECT_EQ(arg->symbol, "a");
    }
    catch (const std::exception& e) {
        FAIL() << "Parser failed on block comments: " << e.what();
    }
}

#pragma once

#include "Expression.hpp"

#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

/*
 * Expression serialization format specification
 * ======================================================================================
 *
 * This serializer uses a recursive, text-based format to represent First-Order Logic
 * expressions. The general syntax for any node is:
 *
 * Keyword[Symbol](Child1, Child2, ...)
 *
 * 1. Components:
 * - Keyword: A case-sensitive identifier determining the type of the expression.
 * - Symbol:  (Optional) An arbitrary string identifier enclosed in brackets [ ].
 * Required only for specific types. If the symbol is empty, it appears as [].
 * - Args:    (Optional) A comma-separated list of child expressions enclosed in ( ).Null
 * Required if the expression can have children, even if the list is empty.
 * The required type of child expressions (formula or term) depends on the keyword.
 *
 * 2. Keyword definitions (default configuration):
 * The table below lists all supported keywords, their underlying types, whether they
 * require a named symbol, the argument type, and the expected number of children (arity).
 * * -----------------------------------------------------------------------------------------
 * Keyword      | Expression type | Symbol | Arity | Arg type | Description / Operator
 * -----------------------------------------------------------------------------------------
 * Null         | -               | No     | 0     | -        | Null / Empty node
 * -----------------------------------------------------------------------------------------
 * True         | BOOLEAN         | No     | 0     | -        | Logical true constant
 * False        | BOOLEAN         | No     | 0     | -        | Logical false constant
 * -----------------------------------------------------------------------------------------
 * Not          | NEGATION        | No     | 1     | Formula  | Logical negation (NOT)
 * -----------------------------------------------------------------------------------------
 * And          | BINARY          | No     | 2     | Formula  | Binary conjunction (AND)
 * Or           | BINARY          | No     | 2     | Formula  | Binary disjunction (OR)
 * Imp          | BINARY          | No     | 2     | Formula  | Implication (=>)
 * Eqv          | BINARY          | No     | 2     | Formula  | Equivalence (<=>)
 * Xor          | BINARY          | No     | 2     | Formula  | Exclusive OR (XOR)
 * -----------------------------------------------------------------------------------------
 * Conjunction  | JUNCTION        | No     | Any   | Formula  | N-ary conjunction (AND)
 * Disjunction  | JUNCTION        | No     | Any   | Formula  | N-ary disjunction (OR)
 * -----------------------------------------------------------------------------------------
 * Forall       | QUANTIFICATION  | Yes    | 1     | Formula  | Universal quantifier. Sym=Var
 * Exists       | QUANTIFICATION  | Yes    | 1     | Formula  | Existential quantifier. Sym=Var
 * -----------------------------------------------------------------------------------------
 * Pred         | PREDICATE       | Yes    | Any   | Term     | Atomic formula / relation
 * Equal        | EQUALITY        | No     | 2     | Term     | Equality between two terms
 * -----------------------------------------------------------------------------------------
 * Func         | FUNCTION        | Yes    | Any   | Term     | Function term
 * Distinct     | FUNCTION        | Yes    | 0     | -        | Distinct object (constant)
 * Var          | VARIABLE        | Yes    | 0     | -        | Variable term
 * -----------------------------------------------------------------------------------------
 *
 * 3. Symbol escaping:
 * To support arbitrary characters within symbols (including brackets), the following
 * escaping rules apply inside the [...] section:
 * - The backslash character '\' is serialized as '\\'.
 * - The closing bracket ']' is serialized as '\]'.
 * - All other characters (including '[') are preserved literally.
 * Example: A symbol named "arr[i]" is serialized as [arr[i\]].
 *
 * 4. Whitespace rules:
 * - Ignored: Around every expression, including the interior of parentheses (...).
 * - Forbidden: Between keyword, symbol [...], and arguments (...). These components
 * must be adjacent.
 * - Literal: Inside symbol [...], whitespace is treated as part of the identifier.
 * 
 * 5. Sequences:
 * Multiple expressions can be processed (on the root level) as a sequence separated
 * by any allowed whitespace (typically newlines).
 *
 * 6. Examples:
 * - Variable:             Var[x]
 * - Function:             Func[f](Var[x], Var[y])
 * - Binary op:            And(Pred[a], Pred[b])
 * - Quantifier:           Forall[x](Pred[P](Var[x]))
 * - Complex symbol:       Var[array[0\]]
 */

class ExpressionSerializer {
public:
    struct KeywordSpec {
        std::string keyword;
        Expression::Type exprType;
        int subType;
    };

    struct Config {
        char symbolOpen = '[';
        char symbolClose = ']';

        char argsOpen = '(';
        char argsClose = ')';
        char argSeparator = ',';

        char space = ' ';
        char newLine = '\n';
        std::string whitespaces = " \n\t\r"; // allowed whitespaces

        std::string trueValue = "True";
        std::string falseValue = "False";

        std::string negation = "Not";

        std::string binaryAnd = "And";
        std::string binaryOr = "Or";
        std::string binaryImp = "Imp";
        std::string binaryEqv = "Eqv";
        std::string binaryXor = "Xor";

        std::string conjunction = "Conjunction";
        std::string disjunction = "Disjunction";

        std::string forall = "Forall";
        std::string exists = "Exists";

        std::string predicate = "Pred";
        std::string equality = "Equal";
        std::string function = "Func";
        std::string distinctObject = "Distinct";
        std::string variable = "Var";

        std::string nullNode = "Null";

        using ET = Expression::Type;
        using BT = BinaryFormula::Operator;
        using JT = JunctionFormula::Operator;
        using QT = QuantificationFormula::Quantifier;

        std::vector<KeywordSpec> keywordSpecs = {
            { trueValue,      ET::BOOLEAN, true },
            { falseValue,     ET::BOOLEAN, false },

            { negation,       ET::NEGATION, 0 },

            { binaryAnd,      ET::BINARY, (int)BT::AND },
            { binaryOr,       ET::BINARY, (int)BT::OR },
            { binaryImp,      ET::BINARY, (int)BT::IMP },
            { binaryEqv,      ET::BINARY, (int)BT::EQV },
            { binaryXor,      ET::BINARY, (int)BT::XOR },

            { conjunction,    ET::JUNCTION, (int)JT::AND },
            { disjunction,    ET::JUNCTION, (int)JT::OR },

            { forall,         ET::QUANTIFICATION, (int)QT::FORALL },
            { exists,         ET::QUANTIFICATION, (int)QT::EXISTS },

            { predicate,      ET::PREDICATE, 0 },
            { equality,       ET::EQUALITY,  0 },
            { function,       ET::FUNCTION,  false },
            { distinctObject, ET::FUNCTION, true },
            { variable,       ET::VARIABLE,  0 }
        };
    };

    struct ParserException : public std::runtime_error {
        std::string msg;
        int line;
        int column;

        ParserException(const std::string& msg, int line, int column);
        std::string getMessage() const;
    private:
        static std::string formatMessage(const std::string& msg, int l, int c);
    };

    static const ExpressionSerializer& getDefault();

	explicit ExpressionSerializer(Config cfg = Config());

    std::string serialize(const ExpressionPtr& expr) const;
    std::string serializeSequence(const std::vector<ExpressionPtr>& exprs) const;
    ExpressionPtr deserialize(const std::string& input) const;
    std::vector<ExpressionPtr> deserializeSequence(const std::string& input) const;

private:
    class CharStream;

	const Config config;
    std::string keywordDelimiters;

    void serializeExpr(const ExpressionPtr& expr, std::ostream& os) const;
    void serializeSymbol(const std::string& symbol, std::ostream& os) const;
    void serializeArgs(const ExpressionPtr& child, std::ostream& os) const;
    void serializeArgs(const ExpressionPtr& left, const ExpressionPtr& right, std::ostream& os) const;
    void serializeArgs(const std::vector<FormulaPtr>& operands, std::ostream& os) const;
    void serializeArgs(const std::vector<TermPtr>& arguments, std::ostream& os) const;

    ExpressionPtr deserializeExpr(CharStream& cs) const; // keyword[symbol](...)
    std::string deserializeKeyword(CharStream& cs) const; // keyword
    std::string deserializeSymbol(CharStream& cs) const; // [symbol]
    ExpressionPtr deserializeChild(CharStream& cs) const; // (child)
    std::pair<ExpressionPtr, ExpressionPtr> deserializePair(CharStream& cs) const; // (left, right)
    std::vector<FormulaPtr> deserializeOperands(CharStream& cs) const; // (oper1, oper2, ...)
    std::vector<TermPtr> deserializeArguments(CharStream& cs) const; // (arg1, arg2, ...)
};

#pragma once

#include "Expression.hpp"

#include <functional>
#include <string>
#include <vector>

class ExpressionPrinter {
public:
    struct Config {
        enum class LatexStyle {
            STANDARD,
            COMPUTER_SCIENCE
        };

        bool explicitPrecedence;
        bool explicitAssociativity;
        bool explicitRightAssociativity;
        bool explicitStructure;

        std::string trueValue;
        std::string falseValue;

        std::string notOp;

        std::string andOp;
        std::string orOp;
        std::string impOp;
        std::string eqvOp;
        std::string xorOp;
        std::string eqOp;

        std::string forallOp;
        std::string existsOp;
        std::string quantSeparator;

        std::string groupOpen;
        std::string groupClose;
        std::string argsOpen;
        std::string argsClose;
        std::string argSeparator;

        std::string nullPlaceholder;
        std::string errorPlaceholder;

        enum class SymbolType {PREDICATE, FUNCTION, VARIABLE, DISTINCT_OBJECT};
        std::function<std::string(const std::string&, SymbolType)> symbolProc;

        /**
         * Configures the printer for a simple, human-readable text format.
         */
        static Config text();

        /**
         * Configures the printer for UTF-8 text format.
         */
        static Config textUtf8();

        /**
         * Configures the printer for LaTeX output.
         * STANDARD produces clean, textbook-style mathematical formulas.
         * COMPUTER_SCIENCE prioritizes explicit structure (parentheses) and uses
         * double-arrow notation, often preferred in formal logic.
         */
        static Config latex(LatexStyle style = LatexStyle::STANDARD);

        /**
         * Configures the printer for the TPTP format.
         * Note that while syntax and precedence are handled automatically, symbol names
         * are printed verbatim. To ensure valid output, variables must start with an
         * uppercase letter, whereas functions and predicates must start with a lowercase
         * letter.
         */
        static Config tptp();
    };

    explicit ExpressionPrinter(Config cfg = Config::latex());

    std::string toString(const ExpressionPtr& expr) const;

private:
    Config config;

    void visit(const Expression* expr, std::ostream& os, bool addParentheses = false) const;
    void visit(const BooleanFormula* f, std::ostream& os) const;
    void visit(const NegationFormula* f, std::ostream& os) const;
    void visit(const BinaryFormula* f, std::ostream& os) const;
    void visit(const JunctionFormula* f, std::ostream& os) const;
    void visit(const QuantificationFormula* f, std::ostream& os) const;
    void visit(const PredicateFormula* f, std::ostream& os) const;
    void visit(const EqualityFormula* f, std::ostream& os) const;
    void visit(const FunctionTerm* t, std::ostream& os) const;
    void visit(const VariableTerm* t, std::ostream& os) const;

    void printArgs(const std::vector<TermPtr>& args, std::ostream& os) const;
    int getOperatorPrecedence(const Expression* expr) const;
    bool shouldPrintParentheses(const Expression* parent, const Expression* child, bool isRightChild = false) const;
};

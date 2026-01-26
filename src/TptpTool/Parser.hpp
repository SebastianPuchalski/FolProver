#pragma once

#include "../Expression.hpp"
#include "Lexer.hpp"
#include "Loader.hpp"

#include <memory>
#include <string>
#include <vector>

namespace TptpTool {

class Parser {
public:
    // Constructor accepts a vector of tokens (moves it for efficiency)
    explicit Parser(std::vector<Token> tokens);

    // Main entry point: Parses the list of tokens into formulas and includes
    Loader::FileParseResult parse();

private:
    const std::vector<Token> tokens_;
    size_t pos_ = 0;

    // --- Navigation & Error Handling ---
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    bool isAtEnd() const;

    // Checks if current token matches type without consuming it
    bool check(TokenType type) const;

    // Consumes token if it matches type, returns true
    bool match(TokenType type);

    // Consumes token or throws an exception
    Token consume(TokenType type, const std::string& msg);

    void advance();

    // Throws a formatted runtime_error
    [[noreturn]] void error(const std::string& msg) const;

    // --- Grammar Rules: Top Level ---
    Loader::IncludeDirective parseInclude();
    Loader::AnnotatedFormula parseAnnotatedFormula();
    std::string parseAnnotations();

    // --- Grammar Rules: Logic ---
    // Implements Pratt Parser (Precedence Climbing)
    FormulaPtr parseLogicFormula(int minBindingPower = 0);
    FormulaPtr parseUnitaryFormula();

    // --- Grammar Rules: Terms ---
    TermPtr parseTerm();
    std::vector<TermPtr> parseTermList();
    std::vector<std::string> parseVariableList();

    std::string normalizeSymbol(const std::string& text) const;
};

} // namespace TptpTool

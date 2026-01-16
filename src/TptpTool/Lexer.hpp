#pragma once

#include <iostream>
#include <string>
#include <vector>

namespace TptpTool {

enum class TokenType {
    // --- Meta ---
    END_OF_FILE, ERROR,

    // --- Keywords ---
    KW_FOF,     // fof
    KW_CNF,     // cnf
    KW_THF,     // thf
    KW_TFF,     // tff
    KW_INCLUDE, // include

    // --- Delimiters ---
    LPAREN, RPAREN,     // ( )
    LBRACKET, RBRACKET, // [ ]
    COMMA, DOT, COLON,  // , . :

    // --- Logical Operators ---
    AND,            // &
    OR,             // |
    NOT,            // ~
    NAND,           // ~&
    NOR,            // ~|
    IMPLIES,        // =>
    IMPLIED_BY,     // <=
    EQUIV,          // <=>
    XOR,            // <~>

    // --- Quantifiers ---
    FORALL,         // !
    EXISTS,         // ?

    // --- Equality ---
    EQUALS,         // =
    NOT_EQUALS,     // !=

    // --- Values / Identifiers ---
    VARIABLE,       // Starts with Upper Case or _ (e.g., X, _A)
    NAME,           // Starts with Lower Case or Single Quoted (e.g., f, 'cat')
    DISTINCT_OBJECT,// Double Quoted (e.g., "Apple")
    DOLLAR_WORD,    // System defined (e.g., $true, $false)

    // --- Numbers ---
    INTEGER,        // 123
    REAL            // 123.45, 1.2E-5
};

struct Token {
    TokenType type;
    std::string text;
    int line;
    int column;

    std::string toString() const;
};

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    const std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    size_t start_ = 0;

    // Navigation helpers
    bool isAtEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);

    // Scanners
    Token scanToken();
    Token scanIdentifier();
    Token scanNumber();
    Token scanQuotedString(char quoteChar, TokenType type);
    Token scanDollar();
    void skipWhitespaceAndComments();

    Token makeToken(TokenType type);
    Token makeToken(TokenType type, std::string text);
    Token errorToken(const std::string& msg);
};

} // namespace TptpTool

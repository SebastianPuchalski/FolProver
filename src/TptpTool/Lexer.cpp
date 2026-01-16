#include "Lexer.hpp"

#include <cctype>
#include <unordered_map>

namespace TptpTool {

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"fof",     TokenType::KW_FOF},
    {"cnf",     TokenType::KW_CNF},
    {"thf",     TokenType::KW_THF},
    {"tff",     TokenType::KW_TFF},
    {"include", TokenType::KW_INCLUDE}
};

std::string Token::toString() const {
    return "Token(" + std::to_string((int)type) + ", '" + text + "')";
}

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!isAtEnd()) {
        start_ = pos_;
        Token token = scanToken();

        if (token.type != TokenType::END_OF_FILE) {
            tokens.push_back(token);
        }
        else {
            break;
        }
    }
    // Append EOF token to simplify parsing logic
    tokens.push_back({ TokenType::END_OF_FILE, "", line_, column_ });
    return tokens;
}

Token Lexer::scanToken() {
    skipWhitespaceAndComments();
    start_ = pos_;

    if (isAtEnd()) return makeToken(TokenType::END_OF_FILE, "");

    char c = advance();

    switch (c) {
        // Single character delimiters
    case '(': return makeToken(TokenType::LPAREN);
    case ')': return makeToken(TokenType::RPAREN);
    case '[': return makeToken(TokenType::LBRACKET);
    case ']': return makeToken(TokenType::RBRACKET);
    case ',': return makeToken(TokenType::COMMA);
    case '.': return makeToken(TokenType::DOT);
    case ':': return makeToken(TokenType::COLON);

        // Basic logical operators
    case '&': return makeToken(TokenType::AND);
    case '|': return makeToken(TokenType::OR);
    case '?': return makeToken(TokenType::EXISTS);

        // Complex operators
    case '~':
        if (match('&')) return makeToken(TokenType::NAND);   // ~&
        if (match('|')) return makeToken(TokenType::NOR);    // ~|
        return makeToken(TokenType::NOT);                    // ~

    case '!':
        if (match('=')) return makeToken(TokenType::NOT_EQUALS); // !=
        return makeToken(TokenType::FORALL);                     // !

    case '=':
        if (match('>')) return makeToken(TokenType::IMPLIES);    // =>
        return makeToken(TokenType::EQUALS);                     // =

    case '-':
        if (isdigit(peek())) return scanNumber();
        return errorToken("Unexpected character: -");

    case '+':
        if (isdigit(peek())) return scanNumber();
        return errorToken("Unexpected character: +");

    case '<':
        if (match('=')) {
            if (match('>')) return makeToken(TokenType::EQUIV);  // <=>
            return makeToken(TokenType::IMPLIED_BY);             // <=
        }
        if (match('~')) {
            if (match('>')) return makeToken(TokenType::XOR);    // <~>
        }
        return errorToken("Unexpected character after <");
    }

    // Identifiers and Literals
    if (c == '\'') return scanQuotedString('\'', TokenType::NAME);
    if (c == '"')  return scanQuotedString('"', TokenType::DISTINCT_OBJECT);
    if (c == '$')  return scanDollar();

    if (isdigit(c)) return scanNumber();
    if (isalpha(c) || c == '_') return scanIdentifier();

    return errorToken(std::string("Unexpected character: ") + c);
}

Token Lexer::scanQuotedString(char quoteChar, TokenType type) {
    while (peek() != quoteChar && !isAtEnd()) {
        if (peek() == '\n') {
            line_++;
            column_ = 1;
        }
        // Handle escaped characters (e.g. \' inside single quotes)
        // We skip the backslash so the quote char doesn't terminate the string.
        if (peek() == '\\' && peekNext() == quoteChar) {
            advance();
        }
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string");

    advance(); // Consume closing quote
    return makeToken(type);
}

Token Lexer::scanIdentifier() {
    while (isalnum(peek()) || peek() == '_') {
        advance();
    }

    std::string text = source_.substr(start_, pos_ - start_);

    // Check against keywords
    auto it = KEYWORDS.find(text);
    if (it != KEYWORDS.end()) {
        return makeToken(it->second);
    }

    // Determine type based on TPTP convention (Upper case = Variable)
    if (isupper(text[0]) || text[0] == '_') {
        return makeToken(TokenType::VARIABLE);
    }

    return makeToken(TokenType::NAME);
}

Token Lexer::scanNumber() {
    while (isdigit(peek())) advance();

    // Fraction part: requires a digit after dot to avoid conflict with End-Of-Formula dot
    if (peek() == '.' && isdigit(peekNext())) {
        advance();
        while (isdigit(peek())) advance();
    }

    // Scientific notation (e.g., 1.2E-10)
    if (peek() == 'E' || peek() == 'e') {
        advance();
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        if (!isdigit(peek())) return errorToken("Expect digits in exponent");
        while (isdigit(peek())) advance();
    }

    // Determine specific numeric type based on presence of dot or exponent
    std::string text = source_.substr(start_, pos_ - start_);
    bool isReal = text.find('.') != std::string::npos ||
        text.find('E') != std::string::npos ||
        text.find('e') != std::string::npos;

    return makeToken(isReal ? TokenType::REAL : TokenType::INTEGER, text);
}

Token Lexer::scanDollar() {
    // System identifiers: $true, $false, etc.
    while (isalnum(peek()) || peek() == '_') {
        advance();
    }
    return makeToken(TokenType::DOLLAR_WORD);
}

void Lexer::skipWhitespaceAndComments() {
    for (;;) {
        char c = peek();
        switch (c) {
        case ' ':
        case '\r':
        case '\t':
            advance();
            break;
        case '\n':
            line_++;
            column_ = 1;
            advance();
            break;
        case '%': // Single line comment
            while (peek() != '\n' && !isAtEnd()) advance();
            break;
        case '/': // Possible block comment
            if (peekNext() == '*') {
                advance(); advance();
                while (!isAtEnd()) {
                    if (peek() == '*' && peekNext() == '/') {
                        advance(); advance();
                        break;
                    }
                    if (peek() == '\n') { line_++; column_ = 1; }
                    advance();
                }
            }
            else {
                return; // Not a comment (just a slash)
            }
            break;
        default:
            return;
        }
    }
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.length();
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}

char Lexer::peekNext() const {
    if (pos_ + 1 >= source_.length()) return '\0';
    return source_[pos_ + 1];
}

char Lexer::advance() {
    column_++;
    return source_[pos_++];
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source_[pos_] != expected) return false;
    pos_++;
    column_++;
    return true;
}

Token Lexer::makeToken(TokenType type) {
    std::string text = source_.substr(start_, pos_ - start_);
    return { type, text, line_, column_ - (int)text.length() };
}

Token Lexer::makeToken(TokenType type, std::string text) {
    return { type, text, line_, column_ - (int)text.length() };
}

Token Lexer::errorToken(const std::string& msg) {
    return { TokenType::ERROR, msg, line_, column_ };
}

} // namespace TptpTool

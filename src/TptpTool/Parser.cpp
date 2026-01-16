#include "Parser.hpp"

#include <sstream>
#include <stdexcept>

namespace TptpTool {

    static const Token EOF_TOKEN{ TokenType::END_OF_FILE, "", 0, 0 };

    // Helper: Strip quotes from single-quoted names if present.
    // Example: 'cat' -> cat, 'file.ax' -> file.ax. If not quoted, returns text as is.
    static std::string stripQuotes(const std::string& text) {
        if (text.length() >= 2 && text.front() == '\'' && text.back() == '\'') {
            return text.substr(1, text.length() - 2);
        }
        return text;
    }

    // --- Operator Precedence Table ---
    static int getBindingPower(TokenType type) {
        switch (type) {
        case TokenType::EQUIV:
        case TokenType::XOR:        return 10;
        case TokenType::IMPLIES:
        case TokenType::IMPLIED_BY: return 20;
        case TokenType::OR:
        case TokenType::NOR:        return 30;
        case TokenType::AND:
        case TokenType::NAND:       return 40;
        default: return 0;
        }
    }

    Parser::Parser(std::vector<Token> tokens)
        : tokens_(std::move(tokens)) {
    }

    Loader::FileParseResult Parser::parse() {
        Loader::FileParseResult result;

        while (!isAtEnd()) {
            if (check(TokenType::KW_INCLUDE)) {
                result.includes.push_back(parseInclude());
            }
            else if (check(TokenType::KW_FOF) || check(TokenType::KW_CNF)) {
                result.formulas.push_back(parseAnnotatedFormula());
            }
            else if (check(TokenType::KW_TFF) || check(TokenType::KW_THF)) {
                error("Unsupported formula type '" + current().text + "'. Only FOF and CNF are supported");
            }
            else {
                error("Unexpected token at top level: " + current().text);
            }
        }
        return result;
    }

    Loader::IncludeDirective Parser::parseInclude() {
        consume(TokenType::KW_INCLUDE, "Expected 'include'");
        consume(TokenType::LPAREN, "Expected '(' after include");

        Loader::IncludeDirective directive;

        // 1. Parse filename
        directive.filePath = stripQuotes(consume(TokenType::NAME, "Expected filename in include").text);

        // 2. Handle optional formula selection list: include('file', [name1, name2])
        if (match(TokenType::COMMA)) {
            consume(TokenType::LBRACKET, "Expected '[' starting formula selection list");

            if (!check(TokenType::RBRACKET)) {
                do {
                    // Formula names can be identifiers, integers, or even keywords
                    if (check(TokenType::NAME) || check(TokenType::INTEGER) ||
                        check(TokenType::KW_FOF) || check(TokenType::KW_CNF) ||
                        check(TokenType::KW_INCLUDE)) {

                        directive.filter.push_back(stripQuotes(current().text));
                        advance();
                    }
                    else {
                        error("Expected formula name in include filter");
                    }
                } while (match(TokenType::COMMA));
            }

            consume(TokenType::RBRACKET, "Expected ']' ending formula selection list");
        }

        consume(TokenType::RPAREN, "Expected ')' after include arguments");
        consume(TokenType::DOT, "Expected '.' after include directive");

        return directive;
    }

    Loader::AnnotatedFormula Parser::parseAnnotatedFormula() {
        Token typeTok = current();
        advance();

        consume(TokenType::LPAREN, "Expected '(' starting formula");

        // 2. Name
        std::string name;
        if (check(TokenType::NAME) || check(TokenType::INTEGER) ||
            check(TokenType::KW_INCLUDE) || check(TokenType::KW_FOF) ||
            check(TokenType::KW_CNF) || check(TokenType::KW_TFF) ||
            check(TokenType::KW_THF)) {

            // CHANGE: Strip quotes
            name = stripQuotes(current().text);
            advance();
        }
        else {
            error("Expected formula name (identifier, integer, or keyword)");
        }

        consume(TokenType::COMMA, "Expected ',' after name");

        // 3. Role
        std::string role;
        if (check(TokenType::NAME) ||
            check(TokenType::KW_INCLUDE) || check(TokenType::KW_FOF) ||
            check(TokenType::KW_CNF) || check(TokenType::KW_TFF) ||
            check(TokenType::KW_THF)) {

            // CHANGE: Strip quotes
            role = stripQuotes(current().text);
            advance();
        }
        else {
            error("Expected formula role");
        }

        consume(TokenType::COMMA, "Expected ',' after role");

        // 4. The Logic Formula
        FormulaPtr formula = parseLogicFormula();

        // 5. Annotations (Optional)
        std::string annotations;
        if (match(TokenType::COMMA)) {
            annotations = parseAnnotations();
        }

        consume(TokenType::RPAREN, "Expected ')' closing annotated formula");
        consume(TokenType::DOT, "Expected '.' ending the entry");

        Loader::AnnotatedFormula out;
        out.type = typeTok.text;
        out.name = name;
        out.role = role;
        out.formula = formula;
        out.annotations = annotations;
        return out;
    }

    std::string Parser::parseAnnotations() {
        std::string result;
        int balance = 0;

        while (!isAtEnd()) {
            if (check(TokenType::RPAREN) && balance == 0) {
                break;
            }

            if (check(TokenType::LPAREN) || check(TokenType::LBRACKET)) {
                balance++;
            }
            else if (check(TokenType::RPAREN) || check(TokenType::RBRACKET)) {
                balance--;
            }

            result += current().text + " ";
            advance();
        }
        return result;
    }

    FormulaPtr Parser::parseLogicFormula(int minBindingPower) {
        FormulaPtr lhs = parseUnitaryFormula();

        while (true) {
            TokenType op = current().type;
            int power = getBindingPower(op);

            if (power == 0 || power < minBindingPower) {
                break;
            }

            advance();

            int nextMinPower = (op == TokenType::IMPLIES || op == TokenType::IMPLIED_BY)
                ? power       // Right-associative
                : power + 1;  // Left-associative

            FormulaPtr rhs = parseLogicFormula(nextMinPower);

            switch (op) {
            case TokenType::AND:
                lhs = std::make_shared<BinaryFormula>(BinaryFormula::Operator::AND, lhs, rhs);
                break;
            case TokenType::OR:
                lhs = std::make_shared<BinaryFormula>(BinaryFormula::Operator::OR, lhs, rhs);
                break;
            case TokenType::IMPLIES:
                lhs = std::make_shared<BinaryFormula>(BinaryFormula::Operator::IMP, lhs, rhs);
                break;
            case TokenType::IMPLIED_BY:
                lhs = std::make_shared<BinaryFormula>(BinaryFormula::Operator::IMP, rhs, lhs);
                break;
            case TokenType::EQUIV:
                lhs = std::make_shared<BinaryFormula>(BinaryFormula::Operator::EQV, lhs, rhs);
                break;
            case TokenType::XOR:
                lhs = std::make_shared<BinaryFormula>(BinaryFormula::Operator::XOR, lhs, rhs);
                break;
            case TokenType::NAND:
            {
                auto andExpr = std::make_shared<BinaryFormula>(BinaryFormula::Operator::AND, lhs, rhs);
                lhs = std::make_shared<NegationFormula>(andExpr);
                break;
            }
            case TokenType::NOR:
            {
                auto orExpr = std::make_shared<BinaryFormula>(BinaryFormula::Operator::OR, lhs, rhs);
                lhs = std::make_shared<NegationFormula>(orExpr);
                break;
            }
            default:
                error("Unknown binary operator encountered");
            }
        }

        return lhs;
    }

    FormulaPtr Parser::parseUnitaryFormula() {
        if (match(TokenType::FORALL) || match(TokenType::EXISTS)) {
            bool isForall = (tokens_[pos_ - 1].type == TokenType::FORALL);

            consume(TokenType::LBRACKET, "Expected '[' after quantifier");
            auto vars = parseVariableList();
            consume(TokenType::RBRACKET, "Expected ']' after variables");
            consume(TokenType::COLON, "Expected ':' after quantifier list");

            FormulaPtr body = parseLogicFormula(0);

            for (auto it = vars.rbegin(); it != vars.rend(); ++it) {
                auto varTerm = std::make_shared<VariableTerm>(*it);
                auto type = isForall ? QuantificationFormula::Quantifier::FORALL
                    : QuantificationFormula::Quantifier::EXISTS;
                body = std::make_shared<QuantificationFormula>(type, varTerm, body);
            }
            return body;
        }

        if (match(TokenType::NOT)) {
            return std::make_shared<NegationFormula>(parseUnitaryFormula());
        }

        if (match(TokenType::LPAREN)) {
            FormulaPtr expr = parseLogicFormula(0);
            consume(TokenType::RPAREN, "Expected ')' after parenthesized expression");
            return expr;
        }

        // System predicates ($true, $false, $ite_f, $distinct)
        if (match(TokenType::DOLLAR_WORD)) {
            std::string txt = tokens_[pos_ - 1].text;
            if (txt == "$true") return std::make_shared<BooleanFormula>(true);
            if (txt == "$false") return std::make_shared<BooleanFormula>(false);

            if (txt == "$ite_f") {
                consume(TokenType::LPAREN, "Expected '(' after $ite_f");

                FormulaPtr condition = parseLogicFormula(0);
                consume(TokenType::COMMA, "Expected ',' after condition");

                FormulaPtr thenBranch = parseLogicFormula(0);
                consume(TokenType::COMMA, "Expected ',' after then-branch");

                FormulaPtr elseBranch = parseLogicFormula(0);
                consume(TokenType::RPAREN, "Expected ')' after $ite_f arguments");

                // Transform to: (Condition => Then) & (~Condition => Else)
                auto partA = std::make_shared<BinaryFormula>(
                    BinaryFormula::Operator::IMP, condition, thenBranch);

                auto notCond = std::make_shared<NegationFormula>(condition);
                auto partB = std::make_shared<BinaryFormula>(
                    BinaryFormula::Operator::IMP, notCond, elseBranch);

                return std::make_shared<BinaryFormula>(
                    BinaryFormula::Operator::AND, partA, partB);
            }

            if (txt == "$distinct") {
                consume(TokenType::LPAREN, "Expected '(' after $distinct");
                auto args = parseTermList();
                consume(TokenType::RPAREN, "Expected ')' after $distinct arguments");

                // $distinct on 0 or 1 element is trivially true
                if (args.size() < 2) {
                    return std::make_shared<BooleanFormula>(true);
                }

                // Generate a conjunction of inequalities: (arg0 != arg1) & (arg0 != arg2) ...
                FormulaPtr result = nullptr;
                for (size_t i = 0; i < args.size(); ++i) {
                    for (size_t j = i + 1; j < args.size(); ++j) {
                        auto eq = std::make_shared<EqualityFormula>(args[i], args[j]);
                        auto neq = std::make_shared<NegationFormula>(eq);

                        if (!result) {
                            result = neq;
                        }
                        else {
                            result = std::make_shared<BinaryFormula>(
                                BinaryFormula::Operator::AND, result, neq);
                        }
                    }
                }
                return result;
            }

            error("Unsupported system formula: " + txt);
        }

        TermPtr leftTerm = parseTerm();

        if (match(TokenType::EQUALS)) {
            TermPtr rightTerm = parseTerm();
            return std::make_shared<EqualityFormula>(leftTerm, rightTerm);
        }
        else if (match(TokenType::NOT_EQUALS)) {
            TermPtr rightTerm = parseTerm();
            auto eq = std::make_shared<EqualityFormula>(leftTerm, rightTerm);
            return std::make_shared<NegationFormula>(eq);
        }

        if (leftTerm->exprType == Expression::Type::FUNCTION) {
            auto func = std::static_pointer_cast<FunctionTerm>(leftTerm);
            return std::make_shared<PredicateFormula>(func->symbol, func->arguments);
        }
        else if (leftTerm->exprType == Expression::Type::VARIABLE) {
            error("Variable '" + std::static_pointer_cast<VariableTerm>(leftTerm)->symbol +
                "' cannot be used as a predicate symbol in FOF.");
        }

        error("Unexpected term used in formula position.");
    }

    TermPtr Parser::parseTerm() {
        if (match(TokenType::VARIABLE)) {
            return std::make_shared<VariableTerm>(tokens_[pos_ - 1].text);
        }

        if (check(TokenType::DOLLAR_WORD)) {
            error("System defined term '" + current().text + "' is not supported yet");
        }

        // Allow Keywords to be parsed as Function Symbols/Constants.
        if (match(TokenType::NAME) || match(TokenType::INTEGER) ||
            match(TokenType::REAL) || match(TokenType::DISTINCT_OBJECT) ||
            match(TokenType::KW_INCLUDE) || match(TokenType::KW_FOF) ||
            match(TokenType::KW_CNF) || match(TokenType::KW_TFF) ||
            match(TokenType::KW_THF)) {

            TokenType type = tokens_[pos_ - 1].type;
            bool isDistinct = (type == TokenType::DISTINCT_OBJECT ||
                type == TokenType::INTEGER ||
                type == TokenType::REAL);

            std::string symbol = stripQuotes(tokens_[pos_ - 1].text);
            std::vector<TermPtr> args;

            // Check for arguments, but strictly forbid them for numbers and distinct objects.
            if (check(TokenType::LPAREN)) {
                if (isDistinct) {
                    error("Numbers and Distinct Objects cannot have arguments");
                }

                advance(); // Consume '('
                args = parseTermList();
                consume(TokenType::RPAREN, "Expected ')' after function arguments");
            }

            return std::make_shared<FunctionTerm>(symbol, args, isDistinct);
        }

        error("Expected Term (Variable, Function, Number), found: " + current().toString());
    }

    std::vector<TermPtr> Parser::parseTermList() {
        std::vector<TermPtr> args;
        if (check(TokenType::RPAREN)) return args;

        do {
            args.push_back(parseTerm());
        } while (match(TokenType::COMMA));

        return args;
    }

    std::vector<std::string> Parser::parseVariableList() {
        std::vector<std::string> vars;
        do {
            Token t = consume(TokenType::VARIABLE, "Expected variable name in quantifier list");
            vars.push_back(t.text);
        } while (match(TokenType::COMMA));
        return vars;
    }

    // --- Helper Methods ---

    const Token& Parser::current() const {
        if (pos_ >= tokens_.size()) return EOF_TOKEN;
        return tokens_[pos_];
    }

    const Token& Parser::peek(int offset) const {
        if (pos_ + offset >= tokens_.size()) return EOF_TOKEN;
        return tokens_[pos_ + offset];
    }

    bool Parser::isAtEnd() const {
        return current().type == TokenType::END_OF_FILE;
    }

    bool Parser::check(TokenType type) const {
        return current().type == type;
    }

    bool Parser::match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    Token Parser::consume(TokenType type, const std::string& msg) {
        if (check(type)) {
            Token t = current();
            advance();
            return t;
        }
        error(msg + ". Found: " + current().text);
    }

    void Parser::advance() {
        if (!isAtEnd()) pos_++;
    }

    [[noreturn]] void Parser::error(const std::string& msg) const {
        Token t = current();
        std::string loc = "Line " + std::to_string(t.line) + ":" + std::to_string(t.column);
        throw std::runtime_error("Parse Error [" + loc + "]: " + msg);
    }

} // namespace TptpTool

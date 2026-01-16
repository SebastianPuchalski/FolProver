#include "ExpressionSerializer.hpp"

#include <cassert>
#include <sstream>

ExpressionSerializer::ParserException::ParserException(
    const std::string& msg, int line, int column)
    : std::runtime_error(formatMessage(msg, line, column)),
    msg(msg), line(line), column(column) {
}

std::string ExpressionSerializer::ParserException::getMessage() const {
    return formatMessage(msg, line, column);
}

std::string ExpressionSerializer::ParserException::formatMessage(
    const std::string& msg, int l, int c) {
    return "Syntax Error at line " + std::to_string(l) +
        ", column " + std::to_string(c) + ": " + msg;
}

class ExpressionSerializer::CharStream {
public:
    CharStream(const std::string& input, char newLine = '\n')
        : input(input), newLine(newLine),
        position(0), line(1), column(1) {}

    bool isEnd() const { return position >= input.size(); }
    size_t getPosition() const { return position; }
    int getLine() const { return line; }
    int getColumn() const { return column; }

    char peek() const {
        if (isEnd()) return '\0';
        return input[position];
    }

    char advance() {
        char c = peek();
        if (!isEnd()) {
            position++;
            if (c == newLine) {
                line++;
                column = 1;
            }
            else {
                column++;
            }
        }
        return c;
    }

    bool match(char expected) {
        if (peek() == expected) {
            advance();
            return true;
        }
        return false;
    }

    void skipChars(const std::string& chars) {
        while (!isEnd()) {
            char c = peek();
            if (chars.find(c) != std::string::npos) {
                advance();
            }
            else break;
        }
    }

    void throwError(const std::string& msg) const {
        throw ParserException(msg, line, column);
    }

private:
    const std::string& input;
    const char newLine;

    size_t position;
    int line;
    int column;
};

const ExpressionSerializer& ExpressionSerializer::getDefault() {
    static ExpressionSerializer instance;
    return instance;
}

ExpressionSerializer::ExpressionSerializer(Config cfg)
    : config(std::move(cfg)) {

    keywordDelimiters += config.symbolOpen;
    keywordDelimiters += config.symbolClose;
    keywordDelimiters += config.argsOpen;
    keywordDelimiters += config.argsClose;
    keywordDelimiters += config.argSeparator;
    keywordDelimiters += config.whitespaces;

    assert(config.symbolOpen != '\\');
    assert(config.symbolClose != '\\');

    assert(config.whitespaces.find('\\') == std::string::npos);
    assert(config.whitespaces.find(config.symbolOpen) == std::string::npos);
    assert(config.whitespaces.find(config.symbolClose) == std::string::npos);
    assert(config.whitespaces.find(config.argsOpen) == std::string::npos);
    assert(config.whitespaces.find(config.argsClose) == std::string::npos);
    assert(config.whitespaces.find(config.argSeparator) == std::string::npos);

    assert(config.whitespaces.find(config.space) != std::string::npos);
    assert(config.whitespaces.find(config.newLine) != std::string::npos);

    assert(config.symbolOpen != config.argsOpen);
    assert(config.symbolClose != config.argsClose);

    for (size_t i = 0; i < config.keywordSpecs.size(); ++i) {
        const auto& ks = config.keywordSpecs[i];
        assert(!ks.keyword.empty());
        assert(ks.keyword.find_first_of(keywordDelimiters) == std::string::npos);

        for (size_t j = i + 1; j < config.keywordSpecs.size(); ++j) {
            assert(ks.keyword != config.keywordSpecs[j].keyword);
        }
        assert(ks.keyword != config.nullNode);
    }
    assert(!config.nullNode.empty());
    assert(config.nullNode.find_first_of(keywordDelimiters) == std::string::npos);
}

std::string ExpressionSerializer::serialize(const ExpressionPtr& expr) const {
    std::stringstream ss;
    serializeExpr(expr, ss);
    return ss.str();
}

std::string ExpressionSerializer::serializeSequence(
    const std::vector<ExpressionPtr>& exprs) const {
    std::stringstream ss;
    for (const auto& expr : exprs) {
        serializeExpr(expr, ss);
        ss << config.newLine;
    }
    return ss.str();
}

ExpressionPtr ExpressionSerializer::deserialize(const std::string& input) const {
    CharStream cs(input, config.newLine);
    auto expr = deserializeExpr(cs);
    cs.skipChars(config.whitespaces);
    if (!cs.isEnd()) {
        cs.throwError("Unexpected characters after the end of expression");
    }
    return expr;
}

std::vector<ExpressionPtr> ExpressionSerializer::deserializeSequence(
    const std::string& input) const {
    std::vector<ExpressionPtr> result;
    CharStream cs(input, config.newLine);
    cs.skipChars(config.whitespaces);
    while (!cs.isEnd()) {
        result.push_back(deserializeExpr(cs));
        cs.skipChars(config.whitespaces);
    }
    return result;
}

void ExpressionSerializer::serializeExpr(const ExpressionPtr& expr, std::ostream& os) const {
    if (!expr) {
        os << config.nullNode;
        return;
    }

    switch (expr->exprType) {
    case Expression::Type::BOOLEAN: {
        auto boolean = std::static_pointer_cast<BooleanFormula>(expr);
        os << (boolean->value ? config.trueValue : config.falseValue);
        break;
    }
    case Expression::Type::NEGATION: {
        auto negation = std::static_pointer_cast<NegationFormula>(expr);
        os << config.negation;
        serializeArgs(negation->child, os);
        break;
    }
    case Expression::Type::BINARY: {
        auto binary = std::static_pointer_cast<BinaryFormula>(expr);
        switch (binary->op) {
        case BinaryFormula::Operator::AND:
            os << config.binaryAnd; break;
        case BinaryFormula::Operator::OR:
            os << config.binaryOr; break;
        case BinaryFormula::Operator::IMP:
            os << config.binaryImp; break;
        case BinaryFormula::Operator::EQV:
            os << config.binaryEqv; break;
        case BinaryFormula::Operator::XOR:
            os << config.binaryXor; break;
        default:
            assert(!"Unknown operator");
        };
        serializeArgs(binary->left, binary->right, os);
        break;
    }
    case Expression::Type::JUNCTION: {
        auto junction = std::static_pointer_cast<JunctionFormula>(expr);
        switch (junction->op) {
        case JunctionFormula::Operator::AND:
            os << config.conjunction; break;
        case JunctionFormula::Operator::OR:
            os << config.disjunction; break;
        default:
            assert(!"Unknown operator");
        };
        serializeArgs(junction->operands, os);
        break;
    }
    case Expression::Type::QUANTIFICATION: {
        auto quantification = std::static_pointer_cast<QuantificationFormula>(expr);
        switch (quantification->type) {
        case QuantificationFormula::Quantifier::FORALL:
            os << config.forall; break;
        case QuantificationFormula::Quantifier::EXISTS:
            os << config.exists; break;
        default:
            assert(!"Unknown quantifier type");
        };
        std::string variableSymbol;
        if (quantification->variable) {
            variableSymbol = quantification->variable->symbol;
        }
        serializeSymbol(variableSymbol, os);
        serializeArgs(quantification->body, os);
        break;
    }
    case Expression::Type::PREDICATE: {
        auto predicate = std::static_pointer_cast<PredicateFormula>(expr);
        os << config.predicate;
        serializeSymbol(predicate->symbol, os);
        serializeArgs(predicate->arguments, os);
        break;
    }
    case Expression::Type::EQUALITY: {
        auto equality = std::static_pointer_cast<EqualityFormula>(expr);
        os << config.equality;
        serializeArgs(equality->left, equality->right, os);
        break;
    }
    case Expression::Type::FUNCTION: {
        auto function = std::static_pointer_cast<FunctionTerm>(expr);
        os << (function->distinct ? config.distinctObject : config.function);
        serializeSymbol(function->symbol, os);
        if (!function->distinct) serializeArgs(function->arguments, os);
        break;
    }
    case Expression::Type::VARIABLE: {
        auto variable = std::static_pointer_cast<VariableTerm>(expr);
        os << config.variable;
        serializeSymbol(variable->symbol, os);
        break;
    }
    default:
        assert(!"Unknown expression type");
    }
}

void ExpressionSerializer::serializeSymbol(
    const std::string& symbol, std::ostream& os) const {
    os << config.symbolOpen;
    for (char c : symbol) {
        if (c == '\\') {
            os << "\\\\";
        }
        else if (c == config.symbolClose) {
            os << '\\' << config.symbolClose;
        }
        else {
            os << c;
        }
    }
    os << config.symbolClose;
}

void ExpressionSerializer::serializeArgs(
    const ExpressionPtr& child, std::ostream& os) const {
    os << config.argsOpen;
    serializeExpr(child, os);
    os << config.argsClose;
}

void ExpressionSerializer::serializeArgs(
    const ExpressionPtr& left, const ExpressionPtr& right, std::ostream& os) const {
    os << config.argsOpen;
    serializeExpr(left, os);
    os << config.argSeparator << config.space;
    serializeExpr(right, os);
    os << config.argsClose;
}

void ExpressionSerializer::serializeArgs(
    const std::vector<FormulaPtr>& operands, std::ostream& os) const {
    os << config.argsOpen;
    for (size_t i = 0; i < operands.size(); ++i) {
        const auto& operand = operands[i];
        serializeExpr(operand, os);
        if (i < operands.size() - 1) {
            os << config.argSeparator << config.space;
        }
    }
    os << config.argsClose;
}

void ExpressionSerializer::serializeArgs(
    const std::vector<TermPtr>& arguments, std::ostream& os) const {
    os << config.argsOpen;
    for (size_t i = 0; i < arguments.size(); ++i) {
        const auto& argument = arguments[i];
        serializeExpr(argument, os);
        if (i < arguments.size() - 1) {
            os << config.argSeparator << config.space;
        }
    }
    os << config.argsClose;
}

ExpressionPtr ExpressionSerializer::deserializeExpr(CharStream& cs) const {
    cs.skipChars(config.whitespaces);
    if (cs.isEnd()) {
        cs.throwError("Unexpected end of string");
    }

    auto keyword = deserializeKeyword(cs);
    if (keyword.empty()) {
        cs.throwError("Empty keyword");
    }
    if (keyword == config.nullNode) {
        return nullptr;
    }

    Expression::Type exprType;
    int subType;

    const KeywordSpec* spec = nullptr;
    for (const auto& ks : config.keywordSpecs) {
        if (ks.keyword == keyword) spec = &ks;
    }
    if (spec) {
        exprType = spec->exprType;
        subType = spec->subType;
    }
    else {
        const size_t MAX_LENGTH = 32;
        if (keyword.size() > MAX_LENGTH) {
            keyword.resize(MAX_LENGTH);
            keyword += "...";
        }
        cs.throwError(keyword + " is not a keyword");
    }
    
    switch (exprType) {
    case Expression::Type::BOOLEAN: {
        return std::make_shared<BooleanFormula>(subType != 0);
    }
    case Expression::Type::NEGATION: {
        auto child = deserializeChild(cs);
        if (child && !child->isFormula()) {
            cs.throwError("Operand is not a formula");
        }
        auto formula = std::static_pointer_cast<Formula>(child);
        return std::make_shared<NegationFormula>(formula);
    }
    case Expression::Type::BINARY: {
        auto op = static_cast<BinaryFormula::Operator>(subType);
        auto pair = deserializePair(cs);
        if (pair.first && !pair.first->isFormula()) {
            cs.throwError("Left operand is not a formula");
        }
        if (pair.second && !pair.second->isFormula()) {
            cs.throwError("Right operand is not a formula");
        }
        auto left = std::static_pointer_cast<Formula>(pair.first);
        auto right = std::static_pointer_cast<Formula>(pair.second);
        return std::make_shared<BinaryFormula>(op, left, right);
    }
    case Expression::Type::JUNCTION: {
        auto op = static_cast<JunctionFormula::Operator>(subType);
        auto operands = deserializeOperands(cs);
        return std::make_shared<JunctionFormula>(op, operands);
    }
    case Expression::Type::QUANTIFICATION: {
        auto type = static_cast<QuantificationFormula::Quantifier>(subType);
        auto variableSymbol = deserializeSymbol(cs);
        auto variable = std::make_shared<VariableTerm>(variableSymbol);
        auto child = deserializeChild(cs);
        if (child && !child->isFormula()) {
            cs.throwError("Quantifier body is not a formula");
        }
        auto body = std::static_pointer_cast<Formula>(child);
        return std::make_shared<QuantificationFormula>(type, variable, body);
    }
    case Expression::Type::PREDICATE: {
        auto symbol = deserializeSymbol(cs);
        auto arguments = deserializeArguments(cs);
        return std::make_shared<PredicateFormula>(symbol, arguments);
    }
    case Expression::Type::EQUALITY: {
        auto pair = deserializePair(cs);
        if (pair.first && !pair.first->isTerm()) {
            cs.throwError("Left operand of equality is not a term");
        }
        if (pair.second && !pair.second->isTerm()) {
            cs.throwError("Right operand of equality is not a term");
        }
        auto left = std::static_pointer_cast<Term>(pair.first);
        auto right = std::static_pointer_cast<Term>(pair.second);
        return std::make_shared<EqualityFormula>(left, right);
    }
    case Expression::Type::FUNCTION: {
        bool isDistinct = (subType != 0);
        auto symbol = deserializeSymbol(cs);
        std::vector<TermPtr> arguments;
        if (!isDistinct) arguments = deserializeArguments(cs);
        return std::make_shared<FunctionTerm>(symbol, arguments, isDistinct);
    }
    case Expression::Type::VARIABLE: {
        auto symbol = deserializeSymbol(cs);
        return std::make_shared<VariableTerm>(symbol);
    }
    default:
        assert(!"Unknown expression type");
        return nullptr;
    }
}

std::string ExpressionSerializer::deserializeKeyword(CharStream& cs) const {
    std::string result;
    while (!cs.isEnd()) {
        char c = cs.peek();
        if (keywordDelimiters.find(c) != std::string::npos) {
            break;
        }
        result += cs.advance();
    }
    return result;
}

std::string ExpressionSerializer::deserializeSymbol(CharStream& cs) const {
    if (!cs.match(config.symbolOpen)) {
        cs.throwError(std::string("Expected ") + config.symbolOpen);
    }
    std::string symbol;
    while (!cs.isEnd() && cs.peek() != config.symbolClose) {
        if (cs.peek() == '\\') {
            cs.advance();
            if (cs.isEnd()) {
                cs.throwError("Unexpected end of string after escape character");
            }
        }
        symbol += cs.advance();
    }
    if (!cs.match(config.symbolClose)) {
        cs.throwError(std::string("Expected ") + config.symbolClose);
    }
    return symbol;
}

ExpressionPtr ExpressionSerializer::deserializeChild(CharStream& cs) const {
    if (!cs.match(config.argsOpen)) {
        cs.throwError(std::string("Expected ") + config.argsOpen);
    }
    auto expr = deserializeExpr(cs);
    cs.skipChars(config.whitespaces);
    if (!cs.match(config.argsClose)) {
        cs.throwError(std::string("Expected ") + config.argsClose);
    }
    return expr;
}

std::pair<ExpressionPtr, ExpressionPtr> ExpressionSerializer::deserializePair(CharStream& cs) const {
    if (!cs.match(config.argsOpen)) {
        cs.throwError(std::string("Expected ") + config.argsOpen);
    }
    auto left = deserializeExpr(cs);
    cs.skipChars(config.whitespaces);
    if (!cs.match(config.argSeparator)) {
        cs.throwError(std::string("Expected ") + config.argSeparator);
    }
    auto right = deserializeExpr(cs);
    cs.skipChars(config.whitespaces);
    if (!cs.match(config.argsClose)) {
        cs.throwError(std::string("Expected ") + config.argsClose);
    }
    return std::pair(left, right);
}

std::vector<FormulaPtr> ExpressionSerializer::deserializeOperands(CharStream& cs) const {
    if (!cs.match(config.argsOpen)) {
        cs.throwError(std::string("Expected ") + config.argsOpen);
    }
    cs.skipChars(config.whitespaces);
    std::vector<FormulaPtr> operands;
    if (cs.match(config.argsClose)) {
        return operands;
    }
    while (true) {
        auto operand = deserializeExpr(cs);
        if (operand && !operand->isFormula()) {
            cs.throwError("Operand is not a formula");
        }
        operands.push_back(std::static_pointer_cast<Formula>(operand));
        cs.skipChars(config.whitespaces);
        if (cs.match(config.argsClose)) {
            break;
        }
        if (!cs.match(config.argSeparator)) {
            cs.throwError(std::string("Expected ") + config.argSeparator);
        }
    }
    return operands;
}

std::vector<TermPtr> ExpressionSerializer::deserializeArguments(CharStream& cs) const {
    if (!cs.match(config.argsOpen)) {
        cs.throwError(std::string("Expected ") + config.argsOpen);
    }
    cs.skipChars(config.whitespaces);
    std::vector<TermPtr> arguments;
    if (cs.match(config.argsClose)) {
        return arguments;
    }
    while (true) {
        auto argument = deserializeExpr(cs);
        if (argument && !argument->isTerm()) {
            cs.throwError("Operand is not a term");
        }
        arguments.push_back(std::static_pointer_cast<Term>(argument));
        cs.skipChars(config.whitespaces);
        if (cs.match(config.argsClose)) {
            break;
        }
        if (!cs.match(config.argSeparator)) {
            cs.throwError(std::string("Expected ") + config.argSeparator);
        }
    }
    return arguments;
}

#include "ExpressionPrinter.hpp"

#include <cassert>
#include <cstdlib>
#include <sstream>

namespace {
    std::string textSymbolProc(const std::string& symbol, ExpressionPrinter::Config::SymbolType type) {
        if (symbol.empty()) return "!!!EMPTY_SYMBOL!!!";
        if (type == ExpressionPrinter::Config::SymbolType::VARIABLE ||
            type == ExpressionPrinter::Config::SymbolType::DISTINCT_OBJECT) {
            return symbol;
        }

        bool safe = true;
        for (char c : symbol) {
            if (!isalnum(static_cast<unsigned char>(c)) && c != '_') {
                safe = false;
                break;
            }
        }
        if (safe) return symbol;

        std::string result;
        result.reserve(symbol.size() + 2);
        result += "'";
        for (char c : symbol) {
            if (c == '\'' || c == '\\') result += '\\';
            result += c;
        }
        result += "'";
        return result;
    }
}

ExpressionPrinter::Config ExpressionPrinter::Config::text() {
    Config c;

    c.explicitPrecedence = true;
    c.explicitAssociativity = false;
    c.explicitRightAssociativity = true;
    c.explicitStructure = false;

    c.trueValue = "TRUE";
    c.falseValue = "FALSE";

    c.notOp = "~";
    c.andOp = " & ";
    c.orOp = " | ";
    c.impOp = " => ";
    c.eqvOp = " <=> ";
    c.xorOp = " XOR ";
    c.eqOp = " = ";

    c.forallOp = "forall ";
    c.existsOp = "exists ";
    c.quantSeparator = ". ";

    c.groupOpen = "(";
    c.groupClose = ")";
    c.argsOpen = "(";
    c.argsClose = ")";
    c.argSeparator = ", ";

    c.nullPlaceholder = "!!!NULL!!!";
    c.errorPlaceholder = "!!!ERROR!!!";

    c.symbolProc = textSymbolProc;
    return c;
}

ExpressionPrinter::Config ExpressionPrinter::Config::textUtf8() {
    Config c = text();

    c.explicitPrecedence = false;
    c.explicitAssociativity = false;
    c.explicitRightAssociativity = false;
    c.explicitStructure = false;

    c.trueValue = u8"\u22A4";
    c.falseValue = u8"\u22A5";

    c.notOp = u8"\u00AC";
    c.andOp = u8" \u2227 ";
    c.orOp = u8" \u2228 ";
    c.impOp = u8" \u21D2 ";
    c.eqvOp = u8" \u21D4 ";
    c.xorOp = u8" \u2295 ";
    c.eqOp = " = ";

    c.forallOp = u8"\u2200";
    c.existsOp = u8"\u2203";
    c.quantSeparator = "";

    return c;
}

ExpressionPrinter::Config ExpressionPrinter::Config::latex(LatexStyle style) {
    Config c;

    c.explicitPrecedence = false;
    c.explicitAssociativity = false;
    c.explicitRightAssociativity = false;
    c.explicitStructure = false;

    c.trueValue = "\\top";
    c.falseValue = "\\bot";

    c.notOp = "\\neg ";
    c.andOp = " \\land ";
    c.orOp = " \\lor ";
    c.xorOp = " \\oplus ";
    c.eqOp = " = ";

    c.forallOp = "\\forall ";
    c.existsOp = "\\exists ";

    c.groupOpen = "\\left(";
    c.groupClose = "\\right)";
    c.argsOpen = "(";
    c.argsClose = ")";
    c.argSeparator = ", ";

    c.nullPlaceholder = "{\\color{red}\\textbf{!!!NULL!!!}}";
    c.errorPlaceholder = "{\\color{red}\\textbf{!!!ERROR!!!}}";

    c.symbolProc = [](const std::string& symbol, SymbolType type) -> std::string {
        if (symbol.empty()) return "!!!EMPTY_SYMBOL!!!";
        static const std::string whiteList = ".:;?!@()[]";
        static const std::string greyList = "\"";
        std::string sanitized;
        sanitized.reserve(symbol.size() * 2);
        for (char c : symbol) {
            if (isalnum(static_cast<unsigned char>(c))) {
                sanitized += c;
            }
            else if (c == '_') {
                sanitized += "\\_";
            }
            else if (c == ' ') {
                sanitized += "\\ ";
            }
            else if (whiteList.find(c) != std::string::npos) {
                sanitized += c;
            }
            else if (greyList.find(c) != std::string::npos) {
                sanitized += c;
            }
            else {
                sanitized += "?";
            }
        }
        if (type == SymbolType::VARIABLE) {
            return "\\mathit{" + sanitized + "}";
        }
        else if (type == SymbolType::DISTINCT_OBJECT) {
            return "\\mathtt{" + sanitized + "}";
        }
        return "\\mathrm{" + sanitized + "}";
    };

    switch (style) {
    case LatexStyle::STANDARD:
        c.impOp = " \\to ";
        c.eqvOp = " \\leftrightarrow ";
        c.quantSeparator = "\\, ";
        break;
    case LatexStyle::COMPUTER_SCIENCE:
        c.impOp = " \\Rightarrow ";
        c.eqvOp = " \\Leftrightarrow ";
        c.quantSeparator = ". ";
        c.explicitPrecedence = true;
        c.explicitRightAssociativity = true;
        break;
    }
    return c;
}

ExpressionPrinter::Config ExpressionPrinter::Config::tptp() {
    Config c;

    c.explicitPrecedence = false;
    c.explicitAssociativity = false;
    c.explicitRightAssociativity = false;
    c.explicitStructure = false;

    c.trueValue = "$true";
    c.falseValue = "$false";

    c.notOp = "~";
    c.andOp = " & ";
    c.orOp = " | ";
    c.impOp = " => ";
    c.eqvOp = " <=> ";
    c.xorOp = " <~> ";
    c.eqOp = " = ";

    c.forallOp = "![";
    c.existsOp = "?[";
    c.quantSeparator = "]: ";

    c.groupOpen = "(";
    c.groupClose = ")";
    c.argsOpen = "(";
    c.argsClose = ")";
    c.argSeparator = ", ";

    c.nullPlaceholder = "$null";
    c.errorPlaceholder = "$error";

    c.symbolProc = [](const std::string& symbol, SymbolType type) -> std::string {
        if (symbol.empty()) return "!!!EMPTY_SYMBOL!!!";

        auto isNumeric = [](const std::string& str) -> bool {
            if (str.empty()) return false;
            bool isMinus = str.front() == '-';
            if (str.size() <= isMinus) return false;
            if (str[isMinus] == '.') return false;
            if (str.back() == '.') return false;
            bool dotSeen = false;
            for (size_t i = isMinus; i < str.size(); ++i) {
                char c = str[i];
                if (c == '.') {
                    if (dotSeen) return false;
                    dotSeen = true;
                }
                else if (c < '0' || c > '9') return false;
            }
            return true;
        };

        if (type == SymbolType::VARIABLE) return symbol;
        if (type == SymbolType::DISTINCT_OBJECT) {
            if ((symbol.size() >= 2 && symbol.front() == '"' && symbol.back() == '"') ||
                isNumeric(symbol)) {
                return symbol;
            }
            std::string result;
            result.reserve(symbol.size() + 2);
            result += "\"";
            for (char c : symbol) {
                if (c == '"' || c == '\\') result += '\\';
                result += c;
            }
            result += '"';
            return result;
        }

        bool safe = islower(static_cast<unsigned char>(symbol[0]));
        for (size_t i = 0; safe && i < symbol.size(); ++i) {
            if (!isalnum(static_cast<unsigned char>(symbol[i])) && symbol[i] != '_') {
                safe = false;
            }
        }
        if (safe) return symbol;
        std::string result = "'";
        result.reserve(symbol.size() + 2);
        for (char c : symbol) {
            if (c == '\'' || c == '\\') result += '\\';
            result += c;
        }
        result += "'";
        return result;
    };

    return c;
}

ExpressionPrinter::ExpressionPrinter(Config cfg)
    : config(std::move(cfg)) {
}

std::string ExpressionPrinter::toString(const ExpressionPtr& expr) const {
    std::stringstream ss;
    visit(expr.get(), ss);
    return ss.str();
}

void ExpressionPrinter::visit(const Expression* expr, std::ostream& os, bool addParentheses) const {
    if (!expr) {
        os << config.nullPlaceholder;
        return;
    }

    if(addParentheses) os << config.groupOpen;

    switch (expr->exprType) {
    case Expression::Type::BOOLEAN:        visit(static_cast<const BooleanFormula*>(expr), os); break;
    case Expression::Type::NEGATION:       visit(static_cast<const NegationFormula*>(expr), os); break;
    case Expression::Type::BINARY:         visit(static_cast<const BinaryFormula*>(expr), os); break;
    case Expression::Type::JUNCTION:       visit(static_cast<const JunctionFormula*>(expr), os); break;
    case Expression::Type::QUANTIFICATION: visit(static_cast<const QuantificationFormula*>(expr), os); break;
    case Expression::Type::PREDICATE:      visit(static_cast<const PredicateFormula*>(expr), os); break;
    case Expression::Type::EQUALITY:       visit(static_cast<const EqualityFormula*>(expr), os); break;
    case Expression::Type::FUNCTION:       visit(static_cast<const FunctionTerm*>(expr), os); break;
    case Expression::Type::VARIABLE:       visit(static_cast<const VariableTerm*>(expr), os); break;
    default:
        os << config.errorPlaceholder;
        assert(false && "Unknown Expression type");
        break;
    }

    if (addParentheses) os << config.groupClose;
}

void ExpressionPrinter::visit(const BooleanFormula* f, std::ostream& os) const {
    os << (f->value ? config.trueValue : config.falseValue);
}

void ExpressionPrinter::visit(const NegationFormula* f, std::ostream& os) const {
    os << config.notOp;
    visit(f->child.get(), os, shouldPrintParentheses(f, f->child.get()));
}

void ExpressionPrinter::visit(const BinaryFormula* f, std::ostream& os) const {
    visit(f->left.get(), os, shouldPrintParentheses(f, f->left.get(), false));

    switch (f->op) {
    case BinaryFormula::Operator::AND: os << config.andOp; break;
    case BinaryFormula::Operator::OR:  os << config.orOp; break;
    case BinaryFormula::Operator::IMP: os << config.impOp; break;
    case BinaryFormula::Operator::EQV: os << config.eqvOp; break;
    case BinaryFormula::Operator::XOR: os << config.xorOp; break;
    default: os << config.errorPlaceholder; assert(false); break;
    }

    visit(f->right.get(), os, shouldPrintParentheses(f, f->right.get(), true));
}

void ExpressionPrinter::visit(const JunctionFormula* f, std::ostream& os) const {
    std::string op;
    std::string value;
    switch (f->op) {
    case JunctionFormula::Operator::AND:
        op = config.andOp;
        value = config.trueValue;
        break;
    case JunctionFormula::Operator::OR: 
        op = config.orOp;
        value = config.falseValue;
        break;
    default: os << config.errorPlaceholder; assert(false); return;
    }

    if (f->operands.empty()) {
        os << value;
        return;
    }

    for (size_t i = 0; i < f->operands.size(); ++i) {
        visit(f->operands[i].get(), os, shouldPrintParentheses(f, f->operands[i].get()));
        if (i < f->operands.size() - 1) {
            os << op;
        }
    }
}

void ExpressionPrinter::visit(const QuantificationFormula* f, std::ostream& os) const {
    switch (f->type) {
    case QuantificationFormula::Quantifier::FORALL: os << config.forallOp; break;
    case QuantificationFormula::Quantifier::EXISTS: os << config.existsOp; break;
    default: os << config.errorPlaceholder; assert(false); break;
    }

    if (f->variable) visit(f->variable.get(), os);
    os << config.quantSeparator;

    visit(f->body.get(), os, shouldPrintParentheses(f, f->body.get()));
}

void ExpressionPrinter::visit(const PredicateFormula* f, std::ostream& os) const {
    if (config.symbolProc) {
        os << config.symbolProc(f->symbol, Config::SymbolType::PREDICATE);
    }
    else os << f->symbol;
    printArgs(f->arguments, os);
}

void ExpressionPrinter::visit(const EqualityFormula* f, std::ostream& os) const {
    visit(f->left.get(), os);
    os << config.eqOp;
    visit(f->right.get(), os);
}

void ExpressionPrinter::visit(const FunctionTerm* t, std::ostream& os) const {
    if (config.symbolProc) {
        auto symbolType = Config::SymbolType::FUNCTION;
        if (t->distinct) symbolType = Config::SymbolType::DISTINCT_OBJECT;
        os << config.symbolProc(t->symbol, symbolType);
    }
    else os << t->symbol;
    printArgs(t->arguments, os);
}

void ExpressionPrinter::visit(const VariableTerm* t, std::ostream& os) const {
    if (config.symbolProc) {
        os << config.symbolProc(t->symbol, Config::SymbolType::VARIABLE);
    }
    else os << t->symbol;
}

void ExpressionPrinter::printArgs(const std::vector<TermPtr>& args,
    std::ostream& os) const {
    if (!args.empty()) {
        os << config.argsOpen;
        for (size_t i = 0; i < args.size(); ++i) {
            visit(args[i].get(), os);
            if (i < args.size() - 1) os << config.argSeparator;
        }
        os << config.argsClose;
    }
}

int ExpressionPrinter::getOperatorPrecedence(const Expression* expr) const {
    // NEG > AND > OR > IMP > EQV
    switch(expr->exprType) {
    case Expression::Type::NEGATION: return 5;
    case Expression::Type::BINARY:
        switch (static_cast<const BinaryFormula*>(expr)->op) {
        case BinaryFormula::Operator::AND: return 4;
        case BinaryFormula::Operator::OR:  return 3;
        case BinaryFormula::Operator::IMP: return 2;
        case BinaryFormula::Operator::EQV: return 1;
        case BinaryFormula::Operator::XOR: return -1; // unknown precedence
        default: return -1;
        }
    case Expression::Type::JUNCTION:
        switch (static_cast<const JunctionFormula*>(expr)->op) {
        case JunctionFormula::Operator::AND: return 4;
        case JunctionFormula::Operator::OR:  return 3;
        default: return -1;
        }
    default: return -1;
    }
}

bool ExpressionPrinter::shouldPrintParentheses(const Expression* parent, const Expression* child, bool isRightChild) const {
    assert(parent);
    if (!child) return false;

    auto requiresParentheses = [](const Expression* child) -> bool {
        return child->exprType == Expression::Type::BINARY ||
            child->exprType == Expression::Type::JUNCTION ||
            child->exprType == Expression::Type::EQUALITY;
        };

    if (config.explicitStructure) {
        if (requiresParentheses(child) ||
            parent->exprType == Expression::Type::QUANTIFICATION ||
            child->exprType == Expression::Type::QUANTIFICATION)
            return true;
    }

    switch (parent->exprType) {
    case Expression::Type::NEGATION:
        return requiresParentheses(child);

    case Expression::Type::QUANTIFICATION:
        return requiresParentheses(child);

    case Expression::Type::BINARY:
    case Expression::Type::JUNCTION: {
        if(!requiresParentheses(child)) return false;
        if (child->exprType == Expression::Type::EQUALITY) return false;

        int parentPrec = getOperatorPrecedence(parent);
        int childPrec = getOperatorPrecedence(child);
        if (parentPrec == -1 || childPrec == -1) return true;

        if (parentPrec == childPrec) {
            if (parent->exprType == Expression::Type::BINARY &&
                static_cast<const BinaryFormula*>(parent)->op == BinaryFormula::Operator::IMP) {
                // right associative operator
                if (!isRightChild) return true;
                return config.explicitRightAssociativity;
            }
            else {
                // associative operator
                return config.explicitAssociativity;
            }
        }

        if (config.explicitPrecedence) return true;
        return childPrec < parentPrec;
    }

    case Expression::Type::BOOLEAN:
    case Expression::Type::PREDICATE:
    case Expression::Type::EQUALITY:
    case Expression::Type::FUNCTION:
    case Expression::Type::VARIABLE:
    default:
        return false;
    }
}

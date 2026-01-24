#include "Lpo.hpp"

#include <cassert>

Lpo::Result Lpo::compare(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    bool isLhsNegation = (lhs->exprType == Expression::Type::NEGATION);
    bool isRhsNegation = (rhs->exprType == Expression::Type::NEGATION);
    ExpressionPtr l = isLhsNegation ? std::static_pointer_cast<NegationFormula>(lhs)->child : lhs;
    ExpressionPtr r = isRhsNegation ? std::static_pointer_cast<NegationFormula>(rhs)->child : rhs;
    Result result = compareRec(l, r);
    if (result == Result::EQUAL && isLhsNegation != isRhsNegation) {
        return isLhsNegation ? Result::GREATER : Result::LESS;
    }
    return result;
}

bool Lpo::isGreater(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    return compare(lhs, rhs) == Result::GREATER;
}

bool Lpo::isGreaterOrEqual(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    auto result = compare(lhs, rhs);
    return result == Result::GREATER || result == Result::EQUAL;
}

bool Lpo::isLess(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    return compare(lhs, rhs) == Result::LESS;
}

bool Lpo::isLessOrEqual(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    auto result = compare(lhs, rhs);
    return result == Result::LESS || result == Result::EQUAL;
}

bool Lpo::isEqual(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    return compare(lhs, rhs) == Result::EQUAL;
}

bool Lpo::isIncomparable(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    return compare(lhs, rhs) == Result::INCOMPARABLE;
}

Lpo::Result Lpo::compareRec(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    auto isComposite = [](const ExpressionPtr& e) {
        return e->exprType == Expression::Type::PREDICATE ||
            e->exprType == Expression::Type::EQUALITY ||
            e->exprType == Expression::Type::FUNCTION;
    };

    assert(isComposite(lhs) || lhs->exprType == Expression::Type::VARIABLE);
    assert(isComposite(rhs) || rhs->exprType == Expression::Type::VARIABLE);

    if (lhs->exprType == Expression::Type::VARIABLE &&
        rhs->exprType == Expression::Type::VARIABLE) {
        if (std::static_pointer_cast<VariableTerm>(lhs)->symbol ==
            std::static_pointer_cast<VariableTerm>(rhs)->symbol)
            return Result::EQUAL;
        return Result::INCOMPARABLE;
    }
    if (lhs->exprType == Expression::Type::VARIABLE && isComposite(rhs)) {
        if (performOccursCheck(rhs, lhs)) return Result::LESS;
        return Result::INCOMPARABLE;
    }
    if (isComposite(lhs) && rhs->exprType == Expression::Type::VARIABLE) {
        if (performOccursCheck(lhs, rhs)) return Result::GREATER;
        return Result::INCOMPARABLE;
    }
    assert(isComposite(lhs) && isComposite(rhs));

    bool rhsGreaterThanLhsArgs = true;
    size_t lhsArgsCount = lhs->getChildCount();
    for (size_t i = 0; i < lhsArgsCount; ++i) {
        auto result = compare(lhs->getChild(i), rhs);
        if (result == Result::GREATER || result == Result::EQUAL) return Result::GREATER;
        if (result != Result::LESS) rhsGreaterThanLhsArgs = false;
    }
    bool lhsGreaterThanRhsArgs = true;
    size_t rhsArgsCount = rhs->getChildCount();
    for (size_t i = 0; i < rhsArgsCount; ++i) {
        auto result = compare(rhs->getChild(i), lhs);
        if (result == Result::GREATER || result == Result::EQUAL) return Result::LESS;
        if (result != Result::LESS) lhsGreaterThanRhsArgs = false;
    }
    if (!rhsGreaterThanLhsArgs && !lhsGreaterThanRhsArgs) return Result::INCOMPARABLE;

    auto result = compareHeads(lhs, rhs);
    assert(result != Result::INCOMPARABLE);
    if (result == Result::GREATER) {
        if (lhsGreaterThanRhsArgs) return Result::GREATER;
        return Result::INCOMPARABLE;
    }
    if (result == Result::LESS) {
        if (rhsGreaterThanLhsArgs) return Result::LESS;
        return Result::INCOMPARABLE;
    }

    assert(lhs->exprType == rhs->exprType);
    assert(lhs->getChildCount() == rhs->getChildCount());
    if (lhs->exprType == Expression::Type::EQUALITY) return compareEqualities(lhs, rhs);
    size_t argsCount = lhs->getChildCount();
    for (size_t i = 0; i < argsCount; ++i) {
        auto result = compareRec(lhs->getChild(i), rhs->getChild(i));
        if (result == Result::INCOMPARABLE) return Result::INCOMPARABLE;
        if (result == Result::GREATER) {
            if (lhsGreaterThanRhsArgs) return Result::GREATER;
            return Result::INCOMPARABLE;
        }
        if (result == Result::LESS) {
            if (rhsGreaterThanLhsArgs) return Result::LESS;
            return Result::INCOMPARABLE;
        }
    }
    return Result::EQUAL;
}

Lpo::Result Lpo::compareHeads(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    auto getTypePrecedence = [](const ExpressionPtr& expr) -> int {
        switch (expr->exprType) {
        case Expression::Type::PREDICATE: return 4;
        case Expression::Type::EQUALITY: return 3;
        case Expression::Type::FUNCTION:
            if (std::static_pointer_cast<FunctionTerm>(expr)->distinct) return 1;
            return 2;
        };
        assert(!"Unexpected expression type");
        return 0;
    };

    auto getSymbol = [](const ExpressionPtr& expr) -> std::string {
        switch (expr->exprType) {
        case Expression::Type::PREDICATE:
            return std::static_pointer_cast<PredicateFormula>(expr)->symbol;
        case Expression::Type::EQUALITY:
            return "";
        case Expression::Type::FUNCTION:
            return std::static_pointer_cast<FunctionTerm>(expr)->symbol;
        };
        assert(!"Unexpected expression type");
        return "";
    };

    auto lhsTypePrecedence = getTypePrecedence(lhs);
    auto rhsTypePrecedence = getTypePrecedence(rhs);
    if (lhsTypePrecedence > rhsTypePrecedence) return Result::GREATER;
    if (lhsTypePrecedence < rhsTypePrecedence) return Result::LESS;
    assert(lhs->exprType == rhs->exprType);

    auto lhsArity = lhs->getChildCount();
    auto rhsArity = rhs->getChildCount();
    if (lhsArity > rhsArity) return Result::GREATER;
    if (lhsArity < rhsArity) return Result::LESS;

    auto result = getSymbol(lhs).compare(getSymbol(rhs));
    if (result > 0) return Result::GREATER;
    if (result < 0) return Result::LESS;
    return Result::EQUAL;
}

Lpo::Result Lpo::compareEqualities(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const {
    assert(lhs->exprType == Expression::Type::EQUALITY);
    assert(rhs->exprType == Expression::Type::EQUALITY);
    const size_t ARITY = 2;

    Result cmpResult[ARITY][ARITY];
    bool lhsMask[ARITY] = {true, true};
    bool rhsMask[ARITY] = {true, true};

    for (size_t i = 0; i < ARITY; ++i) {
        for (size_t j = 0; j < ARITY; ++j) {
            if (lhsMask[i] && rhsMask[j]) {
                auto result = cmpResult[i][j] = compare(lhs->getChild(i), rhs->getChild(j));
                if (result == Result::EQUAL) {
                    lhsMask[i] = rhsMask[j] = false;
                }
            }
        }
    }

    if (!lhsMask[0] && !lhsMask[1]) {
        assert(!rhsMask[0] && !rhsMask[1]);
        return Result::EQUAL;
    }

    bool isGreater = true;
    for (size_t j = 0; j < ARITY; ++j) {
        if (!rhsMask[j]) continue;
        bool covered = false;
        for (size_t i = 0; i < ARITY; ++i) {
            if (lhsMask[i] && cmpResult[i][j] == Result::GREATER) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            isGreater = false;
            break;
        }
    }
    if (isGreater) return Result::GREATER;

    bool isLess = true;
    for (size_t i = 0; i < ARITY; ++i) {
        if (!lhsMask[i]) continue;
        bool covered = false;
        for (size_t j = 0; j < ARITY; ++j) {
            if (rhsMask[j] && cmpResult[i][j] == Result::LESS) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            isLess = false;
            break;
        }
    }
    if (isLess) return Result::LESS;

    return Result::INCOMPARABLE;
}

bool Lpo::performOccursCheck(const ExpressionPtr& composite, const ExpressionPtr& variable) const {
    if (composite->exprType == Expression::Type::VARIABLE) {
        auto compSymbol = std::static_pointer_cast<VariableTerm>(composite)->symbol;
        auto varSymbol = std::static_pointer_cast<VariableTerm>(variable)->symbol;
        return compSymbol == varSymbol;
    }
    size_t count = composite->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (performOccursCheck(composite->getChild(i), variable)) return true;
    }
    return false;
}

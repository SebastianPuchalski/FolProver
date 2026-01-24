#pragma once

#include "Expression.hpp"

class Lpo {
public:
    enum class Result { GREATER, LESS, EQUAL, INCOMPARABLE };
    Result compare(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;

    bool isGreater(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;
    bool isGreaterOrEqual(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;
    bool isLess(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;
    bool isLessOrEqual(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;
    bool isEqual(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;
    bool isIncomparable(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;

private:
    Result compareRec(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;
    Result compareHeads(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;
    Result compareEqualities(const ExpressionPtr& lhs, const ExpressionPtr& rhs) const;
    bool performOccursCheck(const ExpressionPtr& composite, const ExpressionPtr& variable) const;
};

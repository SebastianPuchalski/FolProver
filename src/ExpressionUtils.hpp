#pragma once

#include "Expression.hpp"

#include <set>
#include <unordered_map>

namespace ExpressionUtils {

bool isDag(const ExpressionPtr& expr);
bool isTree(const ExpressionPtr& expr);
bool isFullyDefined(const ExpressionPtr& expr);
bool isArityConsistent(const ExpressionPtr& expr);
bool isCnf(const FormulaPtr& formula);
bool isClause(const FormulaPtr& formula);
bool isJunctionCnf(const FormulaPtr& formula);
bool isNnf(const FormulaPtr& formula);
bool isStandardized(const FormulaPtr& formula);

bool areAlphaEquivalent(const ExpressionPtr& expr1, const ExpressionPtr& expr2);
size_t getExpressionSize(const ExpressionPtr& expr);

bool isVarFreeInExpr(const ExpressionPtr& expr, const std::string& varSymbol);
std::vector<std::string> getFreeVariables(const FormulaPtr& formula);
std::unordered_map<ExpressionPtr, std::set<std::string>>
	getFreeVariablesPerNode(const ExpressionPtr& expr);

} // namespace ExpressionUtils

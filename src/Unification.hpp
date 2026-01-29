#pragma once

#include "Expression.hpp"

#include <map>

namespace Unification {

using Substitution = std::map<std::string, TermPtr>; // variable name -> term

bool unify(const ExpressionPtr& expr1,
	const ExpressionPtr& expr2, Substitution& mgu);
ExpressionPtr substitute(const ExpressionPtr& expr,
	const Substitution& substitution, bool inPlace = false);
bool performOccursCheck(const std::string& varSymbol,
	const ExpressionPtr& expr, const Substitution& substitution);

} // namespace Unification

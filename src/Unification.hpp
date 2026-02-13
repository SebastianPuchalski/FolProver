#pragma once

#include "Expression.hpp"

#include <map>

namespace Unification {

using Substitution = std::map<std::string, TermPtr>; // variable name -> term

bool unify(const ExpressionPtr& expr1,
	const ExpressionPtr& expr2, Substitution& mgu);

bool match(const ExpressionPtr& pattern,
	const ExpressionPtr& target, Substitution& substitution);

std::vector<Substitution> matchCommutative(const ExpressionPtr& pattern,
	const ExpressionPtr& target, const Substitution& substitution);

ExpressionPtr substitute(const ExpressionPtr& expr,
	const Substitution& substitution, bool inPlace = false);

bool performOccursCheck(const std::string& varSymbol,
	const ExpressionPtr& expr, const Substitution& substitution);

bool areEqual(const ExpressionPtr& expr1, const ExpressionPtr& expr2);

} // namespace Unification

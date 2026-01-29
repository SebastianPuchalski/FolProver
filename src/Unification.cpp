#include "Unification.hpp"

namespace Unification {

bool unify(const ExpressionPtr& expr1,const ExpressionPtr& expr2, Substitution& mgu) {
    assert(expr1 && expr2);

    if (expr1->isTerm() && expr2->isTerm()) {
        if ((expr1->exprType == Expression::Type::FUNCTION) &&
            (expr2->exprType == Expression::Type::FUNCTION)) {
            auto function1 = std::static_pointer_cast<FunctionTerm>(expr1);
            auto function2 = std::static_pointer_cast<FunctionTerm>(expr2);
            if (function1->arguments.size() != function2->arguments.size()) return false;
            if (function1->symbol != function2->symbol) return false;
            if (function1->distinct != function2->distinct) return false;
        }
        else {
            auto unwrap = [&](ExpressionPtr e) -> ExpressionPtr {
                while (e->exprType == Expression::Type::VARIABLE) {
                    auto variable = std::static_pointer_cast<VariableTerm>(e);
                    assert(!variable->symbol.empty());
                    auto it = mgu.find(variable->symbol);
                    if (it == mgu.end()) break;
                    e = it->second;
                }
                return e;
                };
            ExpressionPtr e1 = unwrap(expr1);
            ExpressionPtr e2 = unwrap(expr2);
            if ((e1->exprType == Expression::Type::VARIABLE) &&
                (e2->exprType == Expression::Type::VARIABLE)) {
                auto variable1 = std::static_pointer_cast<VariableTerm>(e1);
                auto variable2 = std::static_pointer_cast<VariableTerm>(e2);
                if (variable1->symbol == variable2->symbol) return true;
                mgu[variable1->symbol] = variable2;
                return true;
            }
            else if ((e1->exprType == Expression::Type::VARIABLE) ||
                (e2->exprType == Expression::Type::VARIABLE)) {
                VariableTermPtr variable;
                TermPtr term;
                if (e1->exprType == Expression::Type::VARIABLE) {
                    variable = std::static_pointer_cast<VariableTerm>(e1);
                    term = std::static_pointer_cast<Term>(e2);
                }
                else {
                    variable = std::static_pointer_cast<VariableTerm>(e2);
                    term = std::static_pointer_cast<Term>(e1);
                }
                if (performOccursCheck(variable->symbol, term, mgu)) return false;
                mgu[variable->symbol] = term;
                return true;
            }
            return unify(e1, e2, mgu);
        }
    }
    else {
        assert(std::static_pointer_cast<Formula>(expr1)->isAtom());
        assert(std::static_pointer_cast<Formula>(expr2)->isAtom());
        if (expr1->exprType != expr2->exprType) return false;
        if (expr1->exprType == Expression::Type::PREDICATE) {
            auto predicate1 = std::static_pointer_cast<PredicateFormula>(expr1);
            auto predicate2 = std::static_pointer_cast<PredicateFormula>(expr2);
            if (predicate1->arguments.size() != predicate2->arguments.size()) return false;
            if (predicate1->symbol != predicate2->symbol) return false;
        }
        else if (expr1->exprType != Expression::Type::EQUALITY) {
            assert(false);
            return false;
        }
    }

    size_t count = expr1->getChildCount();
    assert(count == expr2->getChildCount());
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child1 = expr1->getChild(i);
        ExpressionPtr child2 = expr2->getChild(i);
        if (!unify(child1, child2, mgu)) return false;
    }
    return true;
}

ExpressionPtr substitute(const ExpressionPtr& expr,
    const Substitution& substitution, bool inPlace) {
    assert(expr);
    if (!expr) return nullptr;
    assert(expr->exprType != Expression::Type::QUANTIFICATION);

    if (!inPlace) return substitute(expr->clone(), substitution, true);

    if (expr->exprType == Expression::Type::VARIABLE) {
        auto variable = std::static_pointer_cast<VariableTerm>(expr);
        assert(!variable->symbol.empty());
        auto it = substitution.find(variable->symbol);
        if (it != substitution.end()) {
            return substitute(it->second->clone(), substitution, true);
        }
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child = expr->getChild(i);
        ExpressionPtr newChild = substitute(child, substitution, true);
        if (child != newChild) {
            expr->setChild(i, newChild);
        }
    }
    return expr;
}

bool performOccursCheck(const std::string& varSymbol,
    const ExpressionPtr& expr, const Substitution& mgu) {
    assert(expr && !varSymbol.empty());
    if (!expr || varSymbol.empty()) return false;
    assert(expr->exprType != Expression::Type::QUANTIFICATION);

    if (expr->exprType == Expression::Type::VARIABLE) {
        auto variable = std::static_pointer_cast<VariableTerm>(expr);
        assert(!variable->symbol.empty());
        if (variable->symbol == varSymbol) return true;
        auto it = mgu.find(variable->symbol);
        if (it != mgu.end()) {
            return performOccursCheck(varSymbol, it->second, mgu);
        }
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child = expr->getChild(i);
        if (performOccursCheck(varSymbol, child, mgu)) return true;
    }
    return false;
}

} // namespace Unification

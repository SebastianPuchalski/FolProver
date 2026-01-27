#include "NameRegistry.hpp"

#include "ExpressionUtils.hpp"

#include <cassert>

using namespace ExpressionUtils;

NameRegistry::NameRegistry(std::string predicateNamePrefix,
    std::string functionNamePrefix,
    std::string variableNamePrefix) :
    predicateNamePrefix(std::move(predicateNamePrefix)),
    functionNamePrefix(std::move(functionNamePrefix)),
    variableNamePrefix(std::move(variableNamePrefix)),
    predicateNameCounter(0), functionNameCounter(0), variableNameCounter(0) {
}

bool NameRegistry::isPredicateNameRegistered(const std::string& symbol) {
    return reservedPredicateNames.count(symbol) > 0;
}

bool NameRegistry::isFunctionNameRegistered(const std::string& symbol) {
    return reservedFunctionNames.count(symbol) > 0;
}

std::string NameRegistry::getUniquePredicateName() {
    std::string name;
    do {
        name = predicateNamePrefix + std::to_string(++predicateNameCounter);
    } while (isPredicateNameRegistered(name));
    reservedPredicateNames.emplace(name, true);
    return name;
}

std::string NameRegistry::getUniqueFunctionName() {
    std::string name;
    do {
        name = functionNamePrefix + std::to_string(++functionNameCounter);
    } while (isFunctionNameRegistered(name));
    reservedFunctionNames.emplace(name, true);
    return name;
}

std::string NameRegistry::getUniqueVariableName() {
    return variableNamePrefix + std::to_string(++variableNameCounter);
}

void NameRegistry::registerPredAndFuncNames(const ExpressionPtr& expr) {
    assert(isDag(expr));
    std::unordered_set<ExpressionPtr> visited;
    return registerNamesRec(expr, visited);
}

void NameRegistry::clearPredicateNames() {
    predicateNameCounter = 0;
    reservedPredicateNames.clear();
}

void NameRegistry::clearFunctionNames() {
    functionNameCounter = 0;
    reservedFunctionNames.clear();
}

void NameRegistry::clearVariableNames() {
    variableNameCounter = 0;
}

void NameRegistry::registerNamesRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited) {
    if (!expr || visited.count(expr)) return;
    visited.insert(expr);
    if (expr->exprType == Expression::Type::PREDICATE) {
        auto pred = std::static_pointer_cast<PredicateFormula>(expr);
        auto it = reservedPredicateNames.find(pred->symbol);
        if (it != reservedPredicateNames.end() && it->second) {
            assert(!"Predicate symbol name already reserved");
            // register all symbols before getUniquePredicateName()
        }
        reservedPredicateNames.emplace(pred->symbol, false);
    }
    else if (expr->exprType == Expression::Type::FUNCTION) {
        auto func = std::static_pointer_cast<FunctionTerm>(expr);
        if (!func->distinct) {
            auto it = reservedFunctionNames.find(func->symbol);
            if (it != reservedFunctionNames.end() && it->second) {
                assert(!"Function symbol name already reserved");
                // register all symbols before getUniqueFunctionName()
            }
            reservedFunctionNames.emplace(func->symbol, false);
        }
    }
    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        registerNamesRec(expr->getChild(i), visited);
    }
}

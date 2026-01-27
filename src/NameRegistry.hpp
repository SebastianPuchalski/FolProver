#pragma once

#include "Expression.hpp"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

class NameRegistry {
public:
    NameRegistry(
        std::string predicateNamePrefix = "p",
        std::string functionNamePrefix = "sk",
        std::string variableNamePrefix = "V");

    bool isPredicateNameRegistered(const std::string& symbol);
    bool isFunctionNameRegistered(const std::string& symbol);

    std::string getUniquePredicateName();
    std::string getUniqueFunctionName();
    std::string getUniqueVariableName();

    void registerPredAndFuncNames(const ExpressionPtr& expr);

    void clearPredicateNames();
    void clearFunctionNames();
    void clearVariableNames();

private:
    const std::string predicateNamePrefix;
    const std::string functionNamePrefix;
    const std::string variableNamePrefix;

    uint64_t predicateNameCounter;
    uint64_t functionNameCounter;
    uint64_t variableNameCounter;

    std::unordered_map<std::string, bool> reservedPredicateNames;
    std::unordered_map<std::string, bool> reservedFunctionNames;

    void registerNamesRec(const ExpressionPtr& expr,
        std::unordered_set<ExpressionPtr>& visited);
};

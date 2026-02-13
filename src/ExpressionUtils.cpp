#include "ExpressionUtils.hpp"

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace ExpressionUtils {

bool isDagRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited,
    std::unordered_set<ExpressionPtr>& stack);
bool isTreeRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited);
bool isFullyDefinedRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited);
bool isArityConsistentRec(const ExpressionPtr& expr,
    std::unordered_map<std::string, size_t>& predArities,
    std::unordered_map<std::string, size_t>& funcArities);
bool isCnfRec(const FormulaPtr& formula);
bool isClauseRec(const FormulaPtr& formula);
bool isNnfRec(const FormulaPtr& formula);
bool isStandardizedRec(const ExpressionPtr& expr,
    std::unordered_set<std::string>& seenNames);

bool areAlphaEquivalentRec(const ExpressionPtr& expr1,
    const ExpressionPtr& expr2, std::map<std::string, std::string>& alphaMap);
size_t getExpressionSizeRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited);

bool isVarFreeInExprRec(const ExpressionPtr& expr,
    const std::string& varSymbol);
void buildFreeVarsCacheRec(const ExpressionPtr& expr,
    std::unordered_map<ExpressionPtr, std::set<std::string>>& cache);

//------------------------------------------------------------------------------

bool isDag(const ExpressionPtr& expr) {
    std::unordered_set<ExpressionPtr> visited;
    std::unordered_set<ExpressionPtr> stack;
    return isDagRec(expr, visited, stack);
}

bool isTree(const ExpressionPtr& expr) {
    std::unordered_set<ExpressionPtr> visited;
    return isTreeRec(expr, visited);
}

bool isFullyDefined(const ExpressionPtr& expr) {
    // no empty nodes (nullptr) and empty symbol strings ("")
    std::unordered_set<ExpressionPtr> visited;
    return isFullyDefinedRec(expr, visited);
}

bool isArityConsistent(const ExpressionPtr& expr) {
    assert(isTree(expr) && isFullyDefined(expr));
    std::unordered_map<std::string, size_t> predArities;
    std::unordered_map<std::string, size_t> funcArities;
    return isArityConsistentRec(expr, predArities, funcArities);
}

bool isCnf(const FormulaPtr& formula) {
    if (!isTree(formula) || !isFullyDefined(formula)) return false;
    return isCnfRec(formula);
}

bool isClause(const FormulaPtr& formula) {
    if (!isTree(formula) || !isFullyDefined(formula)) return false;
    return isClauseRec(formula);
}

bool isJunctionCnf(const FormulaPtr& formula) {
    // Junction AND of junctions OR of literals
    if (!isTree(formula) || !isFullyDefined(formula)) return false;

    if (formula->exprType != Expression::Type::JUNCTION) return false;
    auto junctionAnd = std::static_pointer_cast<JunctionFormula>(formula);
    if (junctionAnd->op != JunctionFormula::Operator::AND) return false;

    for (const auto& clause : junctionAnd->operands) {
        if (clause->exprType != Expression::Type::JUNCTION) return false;
        auto junctionOr = std::static_pointer_cast<JunctionFormula>(clause);
        if (junctionOr->op != JunctionFormula::Operator::OR) return false;
        for (const auto& literal : junctionOr->operands) {
            if (!literal->isLiteral()) return false;
        }
    }
    return true;
}

bool isNnf(const FormulaPtr& formula) {
    if (!isTree(formula) || !isFullyDefined(formula)) return false;
    return isNnfRec(formula);
}

bool isStandardized(const FormulaPtr& formula) {
    if (!isTree(formula) || !isFullyDefined(formula)) return false;
    std::vector<std::string> freeVars = getFreeVariables(formula);
    std::unordered_set<std::string> seenNames(freeVars.begin(), freeVars.end());
    return isStandardizedRec(formula, seenNames);
}

bool areAlphaEquivalent(const ExpressionPtr& expr1, const ExpressionPtr& expr2) {
    assert(isDag(expr1) && isFullyDefined(expr1)); // may cause slowdown in debug mode
    assert(isDag(expr2) && isFullyDefined(expr2)); // may cause slowdown in debug mode
    std::map<std::string, std::string> alphaMap;
    return areAlphaEquivalentRec(expr1, expr2, alphaMap);
}

size_t getExpressionSize(const ExpressionPtr& expr) {
    assert(isDag(expr));
    std::unordered_set<ExpressionPtr> visited;
    return getExpressionSizeRec(expr, visited);
}

bool isVarFreeInExpr(const ExpressionPtr& expr,
    const std::string& varSymbol) {
    assert(isTree(expr) && isFullyDefined(expr)); // may cause slowdown in debug mode
    return isVarFreeInExprRec(expr, varSymbol);
}

std::vector<std::string> getFreeVariables(const FormulaPtr& formula) {
    assert(isTree(formula) && isFullyDefined(formula));
    std::unordered_map<ExpressionPtr, std::set<std::string>> cache;
    buildFreeVarsCacheRec(formula, cache);
    const auto& varsSet = cache[formula];
    return std::vector<std::string>(varsSet.begin(), varsSet.end());
}

std::unordered_map<ExpressionPtr, std::set<std::string>> getFreeVariablesPerNode(const ExpressionPtr& expr) {
    std::unordered_map<ExpressionPtr, std::set<std::string>> nodeToFreeVars;
    buildFreeVarsCacheRec(expr, nodeToFreeVars);
    return nodeToFreeVars;
}

bool isDagRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited, std::unordered_set<ExpressionPtr>& stack) {
    if (!expr) return true;

    if (stack.count(expr)) return false;
    if (visited.count(expr)) return true;
    stack.insert(expr);
    visited.insert(expr);

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isDagRec(expr->getChild(i), visited, stack)) return false;
    }

    stack.erase(expr);
    return true;
}

bool isTreeRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited) {
    if (!expr) return true;

    if (visited.count(expr)) return false;
    visited.insert(expr);

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        if (!isTreeRec(quant->variable, visited)) return false;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isTreeRec(expr->getChild(i), visited)) return false;
    }
    return true;
}

bool isFullyDefinedRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited) {
    if (!expr) return false;

    if (visited.count(expr)) return true;
    visited.insert(expr);

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        if (!isFullyDefinedRec(quant->variable, visited)) return false;
    }

    if (expr->exprType == Expression::Type::PREDICATE) {
        auto pred = std::static_pointer_cast<PredicateFormula>(expr);
        if (pred->symbol.empty()) return false;
    }
    else if (expr->exprType == Expression::Type::FUNCTION) {
        auto func = std::static_pointer_cast<FunctionTerm>(expr);
        if (func->symbol.empty()) return false;
    }
    else if (expr->exprType == Expression::Type::VARIABLE) {
        auto var = std::static_pointer_cast<VariableTerm>(expr);
        if (var->symbol.empty()) return false;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isFullyDefinedRec(expr->getChild(i), visited)) return false;
    }
    return true;
}

bool isArityConsistentRec(const ExpressionPtr& expr,
    std::unordered_map<std::string, size_t>& predArities,
    std::unordered_map<std::string, size_t>& funcArities) {
    if (!expr) return true;

    if (expr->exprType == Expression::Type::PREDICATE) {
        auto pred = std::static_pointer_cast<PredicateFormula>(expr);
        size_t currentArity = pred->getChildCount();
        auto it = predArities.find(pred->symbol);
        if (it != predArities.end()) {
            if (it->second != currentArity) return false;
        }
        else {
            predArities[pred->symbol] = currentArity;
        }
    }
    else if (expr->exprType == Expression::Type::FUNCTION) {
        auto func = std::static_pointer_cast<FunctionTerm>(expr);
        if (func->distinct) return true; // always 0
        size_t currentArity = func->getChildCount();
        auto it = funcArities.find(func->symbol);
        if (it != funcArities.end()) {
            if (it->second != currentArity) return false;
        }
        else {
            funcArities[func->symbol] = currentArity;
        }
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isArityConsistentRec(expr->getChild(i), predArities, funcArities)) {
            return false;
        }
    }
    return true;
}

bool isCnfRec(const FormulaPtr& formula) {
    assert(formula);
    if (!formula) return false;

    if (formula->exprType == Expression::Type::JUNCTION) {
        auto junction = std::static_pointer_cast<JunctionFormula>(formula);
        if (junction->op == JunctionFormula::Operator::AND) {
            for (const auto& child : junction->operands) {
                if (!isCnfRec(child)) return false;
            }
            return true;
        }
    }
    else if (formula->exprType == Expression::Type::BINARY) {
        auto binary = std::static_pointer_cast<BinaryFormula>(formula);
        if (binary->op == BinaryFormula::Operator::AND) {
            return isCnfRec(binary->left) && isCnfRec(binary->right);
        }
    }
    return isClauseRec(formula);
}

bool isClauseRec(const FormulaPtr& formula) {
    assert(formula);
    if (!formula) return false;

    if (formula->exprType == Expression::Type::JUNCTION) {
        auto junction = std::static_pointer_cast<JunctionFormula>(formula);
        if (junction->op == JunctionFormula::Operator::OR) {
            for (const auto& child : junction->operands) {
                if (!isClauseRec(child)) return false;
            }
            return true;
        }
    }
    else if (formula->exprType == Expression::Type::BINARY) {
        auto binary = std::static_pointer_cast<BinaryFormula>(formula);
        if (binary->op == BinaryFormula::Operator::OR) {
            return isClauseRec(binary->left) && isClauseRec(binary->right);
        }
    }
    return formula->isLiteral();
}

bool isNnfRec(const FormulaPtr& formula) {
    assert(formula && "Formula pointer is null");
    if (!formula) return false;

    if (formula->isAtom()) return true;
    if (formula->exprType == Expression::Type::NEGATION) {
        auto neg = std::static_pointer_cast<NegationFormula>(formula);
        return neg->child->isAtom();
    }
    if (formula->exprType == Expression::Type::BINARY) {
        auto bin = std::static_pointer_cast<BinaryFormula>(formula);
        if (bin->op != BinaryFormula::Operator::AND &&
            bin->op != BinaryFormula::Operator::OR) {
            return false;
        }
    }

    size_t count = formula->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        auto child = formula->getChild(i);
        if (child->isFormula()) {
            if (!isNnfRec(std::static_pointer_cast<Formula>(child))) return false;
        }
    }
    return true;
}

bool isStandardizedRec(const ExpressionPtr& expr,
    std::unordered_set<std::string>& seenNames) {
    assert(expr && "Expression pointer is null");
    if (!expr) return false;

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        const std::string& varSymbol = quant->variable->symbol;
        if (seenNames.count(varSymbol)) {
            return false;
        }
        seenNames.insert(varSymbol);
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (!isStandardizedRec(expr->getChild(i), seenNames)) {
            return false;
        }
    }
    return true;
}

bool areAlphaEquivalentRec(const ExpressionPtr& expr1,
    const ExpressionPtr& expr2, std::map<std::string, std::string>& alphaMap) {
    if (expr1 == expr2) return true;
    if (!expr1 || !expr2) return false;
    if (expr1->exprType != expr2->exprType) return false;

    switch (expr1->exprType) {
    case Expression::Type::BOOLEAN:
        if (std::static_pointer_cast<BooleanFormula>(expr1)->value !=
            std::static_pointer_cast<BooleanFormula>(expr2)->value) return false;
        break;
    case Expression::Type::NEGATION:
        break;
    case Expression::Type::BINARY:
        if (std::static_pointer_cast<BinaryFormula>(expr1)->op !=
            std::static_pointer_cast<BinaryFormula>(expr2)->op) return false;
        break;
    case Expression::Type::JUNCTION:
        if (std::static_pointer_cast<JunctionFormula>(expr1)->op !=
            std::static_pointer_cast<JunctionFormula>(expr2)->op) return false;
        break;
    case Expression::Type::QUANTIFICATION: {
        auto quant1 = std::static_pointer_cast<QuantificationFormula>(expr1);
        auto quant2 = std::static_pointer_cast<QuantificationFormula>(expr2);
        if (quant1->type != quant2->type) return false;
        const std::string& symbol1 = quant1->variable->symbol;
        const std::string& symbol2 = quant2->variable->symbol;
        std::string oldMapping;
        if (auto it = alphaMap.find(symbol1); it != alphaMap.end()) {
            oldMapping = it->second;
        }
        alphaMap[symbol1] = symbol2;
        bool result = areAlphaEquivalentRec(quant1->body, quant2->body, alphaMap);
        if (!oldMapping.empty()) alphaMap[symbol1] = oldMapping;
        else alphaMap.erase(symbol1);
        return result;
    }
    case Expression::Type::PREDICATE:
        if (std::static_pointer_cast<PredicateFormula>(expr1)->symbol !=
            std::static_pointer_cast<PredicateFormula>(expr2)->symbol) return false;
        break;
    case Expression::Type::EQUALITY:
        break;
    case Expression::Type::FUNCTION: {
        auto func1 = std::static_pointer_cast<FunctionTerm>(expr1);
        auto func2 = std::static_pointer_cast<FunctionTerm>(expr2);
        if (func1->symbol != func2->symbol) return false;
        if (func1->distinct != func2->distinct) return false;
        break;
    }
    case Expression::Type::VARIABLE: {
        const auto& var1Symbol = std::static_pointer_cast<VariableTerm>(expr1)->symbol;
        const auto& var2Symbol = std::static_pointer_cast<VariableTerm>(expr2)->symbol;
        if (auto it = alphaMap.find(var1Symbol); it != alphaMap.end()) {
            return it->second == var2Symbol;
        }
        return var1Symbol == var2Symbol;
    }
    default:
        assert(!"Unsupported expression type");
        break;
    }

    size_t count = expr1->getChildCount();
    if (count != expr2->getChildCount()) return false;
    for (size_t i = 0; i < count; ++i) {
        if (!areAlphaEquivalentRec(expr1->getChild(i), expr2->getChild(i), alphaMap)) return false;
    }
    return true;
}

size_t getExpressionSizeRec(const ExpressionPtr& expr,
    std::unordered_set<ExpressionPtr>& visited) {
    if (!expr || visited.count(expr)) return 0;
    visited.insert(expr);
    size_t size = 1;
    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        size += getExpressionSizeRec(quant->variable, visited);
    }
    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        size += getExpressionSizeRec(expr->getChild(i), visited);
    }
    return size;
}

bool isVarFreeInExprRec(const ExpressionPtr& expr,
    const std::string& varSymbol) {
    assert(expr && !varSymbol.empty());
    if (!expr || varSymbol.empty()) return false;

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        if (quant->variable->symbol == varSymbol) {
            return false;
        }
    }
    else if (expr->exprType == Expression::Type::VARIABLE) {
        auto variable = std::static_pointer_cast<VariableTerm>(expr);
        assert(!variable->symbol.empty());
        return variable->symbol == varSymbol;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        if (isVarFreeInExprRec(expr->getChild(i), varSymbol)) return true;
    }
    return false;
}

void buildFreeVarsCacheRec(const ExpressionPtr& expr,
    std::unordered_map<ExpressionPtr, std::set<std::string>>& cache) {
    assert(expr && "Expression pointer is null");
    if (!expr || cache.count(expr)) return;

    std::set<std::string> vars;

    if (expr->exprType == Expression::Type::VARIABLE) {
        auto var = std::static_pointer_cast<VariableTerm>(expr);
        vars.insert(var->symbol);
    }
    else if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        buildFreeVarsCacheRec(quant->body, cache);
        const auto& bodyVars = cache[quant->body];
        vars.insert(bodyVars.begin(), bodyVars.end());
        vars.erase(quant->variable->symbol);
    }
    else {
        size_t count = expr->getChildCount();
        for (size_t i = 0; i < count; ++i) {
            auto child = expr->getChild(i);
            buildFreeVarsCacheRec(child, cache);
            const auto& childVars = cache[child];
            vars.insert(childVars.begin(), childVars.end());
        }
    }
    cache[expr] = std::move(vars);
}

} // namespace ExpressionUtils

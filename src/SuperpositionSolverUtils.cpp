#include "SuperpositionSolverUtils.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace SuperpositionSolverUtils {

//------------------------------------------------------------------------------

LiteralSelector::LiteralSelector(SelectorFunc selectorFunc, const Lpo& lpo) :
    selectorFunc(std::move(selectorFunc)), lpo(lpo) {
}

Mask LiteralSelector::selectLiterals(
    const Literals& literals) const {
    // The mask must contain at least one negative literal if non-empty
    if(selectorFunc) return selectorFunc(literals);
    return Mask(literals.size(), false);
}

Mask LiteralSelector::areEligibleForResolution(
    const Literals& literals,
    const Mask& selectionMask) const {

    auto computeMaximalLiterals = [this](
        const Literals& literals,
        const Mask& scopeMask) -> Mask {
        Mask resultMask = scopeMask;
        for (size_t i = 0; i < literals.size(); ++i) {
            if (scopeMask[i]) {
                for (size_t j = 0; j < literals.size(); ++j) {
                    if (i != j && resultMask[j]) {
                        if (lpo.isGreater(literals[j], literals[i])) {
                            resultMask[i] = false;
                            break;
                        }
                    }
                }
            }
        }
        return resultMask;
    };

    assert(literals.size() == selectionMask.size());
    bool isSelectionEmpty = true;
    for (bool bit : selectionMask) {
        if (bit) { isSelectionEmpty = false; break; }
    }
    if (isSelectionEmpty) {
        return computeMaximalLiterals(literals, Mask(literals.size(), true));
    }

    Mask negLiteralsMask(literals.size(), false);
    Mask posLiteralsMask(literals.size(), false);
    for (size_t i = 0; i < literals.size(); ++i) {
        if (selectionMask[i]) {
            if (literals[i]->exprType == Expression::Type::NEGATION) {
                negLiteralsMask[i] = true;
            }
            else posLiteralsMask[i] = true;
        }
    }
    Mask negMaxLiteralsMask = computeMaximalLiterals(literals, negLiteralsMask);
    Mask posMaxLiteralsMask = computeMaximalLiterals(literals, posLiteralsMask);
    for (size_t i = 0; i < literals.size(); ++i) {
        negMaxLiteralsMask[i] = negMaxLiteralsMask[i] || posMaxLiteralsMask[i];
    }
    return negMaxLiteralsMask;
}

Mask LiteralSelector::areEligibleForParamodulation(
    const Literals& literals,
    const Mask& selectionMask,
    bool strictlyMaximal) const {

    assert(literals.size() == selectionMask.size());
    Mask result(literals.size(), false);
    for (bool bit : selectionMask) {
        if (bit) return result;
    }

    if (strictlyMaximal) {
        size_t candidate = 0;
        for (size_t i = 1; i < literals.size(); ++i) {
            if (lpo.isGreater(literals[i], literals[candidate])) {
                candidate = i;
            }
        }
        if (literals[candidate]->exprType == Expression::Type::NEGATION) {
            return result;
        }
        for (size_t i = 0; i < literals.size(); ++i) {
            if (i == candidate) continue;
            if (!lpo.isGreater(literals[candidate], literals[i])) {
                return result;
            }
        }
        result[candidate] = true;
    }
    else {
        for (size_t i = 0; i < literals.size(); ++i) {
            if (literals[i]->exprType != Expression::Type::NEGATION) {
                result[i] = true;
                for (size_t j = 0; j < literals.size(); ++j) {
                    if (i != j) {
                        if (lpo.isGreater(literals[j], literals[i])) {
                            result[i] = false;
                            break;
                        }
                    }
                }
            }
        }
    }
    return result;
}

Mask LiteralSelector::selectNothing(const Literals& literals) {
    return Mask(literals.size(), false);
}

Mask LiteralSelector::selectDiffNegLiteral(const Literals& literals) {
    constexpr float PREDICATE_WEIGHT = 2.0f;
    constexpr float TIE_BREAKER_COEF = 0.01f;

    Mask selection(literals.size(), false);
    float maxMetrics = -1.0f;
    int selectedIndex = -1;

    for (size_t i = 0; i < literals.size(); ++i) {
        if (literals[i]->exprType != Expression::Type::NEGATION) continue;
        auto atom = std::static_pointer_cast<NegationFormula>(literals[i])->child;
        float difference;
        float weight;
        if (atom->exprType == Expression::Type::EQUALITY) {
            auto equality = std::static_pointer_cast<EqualityFormula>(atom);
            float leftWeight = getExpressionWeight(equality->left);
            float rightWeight = getExpressionWeight(equality->right);
            difference = std::abs(leftWeight - rightWeight);
            weight = leftWeight + rightWeight;
        }
        else {
            float predWeight = getExpressionWeight(atom);
            difference = std::abs(predWeight - PREDICATE_WEIGHT);
            weight = predWeight + PREDICATE_WEIGHT;
        }
        float metric = difference + (weight * TIE_BREAKER_COEF);
        if (metric > maxMetrics) {
            maxMetrics = metric;
            selectedIndex = static_cast<int>(i);
        }
    }

    if (selectedIndex != -1) {
        selection[selectedIndex] = true;
    }
    return selection;
}

Mask LiteralSelector::selectComplex(const Literals& literals) {
    for (size_t i = 0; i < literals.size(); ++i) {
        if (literals[i]->exprType == Expression::Type::NEGATION) {
            auto negation = std::static_pointer_cast<NegationFormula>(literals[i]);
            if (negation->child->exprType == Expression::Type::EQUALITY) {
                auto equality = std::static_pointer_cast<EqualityFormula>(negation->child);
                if (equality->left->exprType == Expression::Type::VARIABLE &&
                    equality->right->exprType == Expression::Type::VARIABLE) {
                    Mask selection(literals.size(), false);
                    selection[i] = true;
                    return selection;
                }
            }
        }
    }

    std::function<bool(const ExpressionPtr&)> isGround =
        [&](const ExpressionPtr& expr) -> bool {
        if (expr->exprType == Expression::Type::VARIABLE) return false;
        size_t count = expr->getChildCount();
        for (size_t i = 0; i < count; ++i) {
            if (!isGround(expr->getChild(i))) return false;
        }
        return true;
    };
    float minWeight = std::numeric_limits<float>::max();
    int bestIndex = -1;
    for (size_t i = 0; i < literals.size(); ++i) {
        if (literals[i]->exprType == Expression::Type::NEGATION) {
            auto negation = std::static_pointer_cast<NegationFormula>(literals[i]);
            auto atom = negation->child;
            if (isGround(atom)) {
                float weight = getExpressionWeight(atom);
                if (weight < minWeight) {
                    minWeight = weight;
                    bestIndex = static_cast<int>(i);
                }
            }
        }
    }
    if (bestIndex != -1) {
        Mask selection(literals.size(), false);
        selection[bestIndex] = true;
        return selection;
    }

    return selectDiffNegLiteral(literals);
}

Mask LiteralSelector::selectComplexExceptRRHorn(const Literals& literals) {
    FormulaPtr positiveLiteral = nullptr;
    for (const auto& literal : literals) {
        if (literal->exprType != Expression::Type::NEGATION) {
            if (positiveLiteral) return selectComplex(literals);
            positiveLiteral = literal;
        }
    }

    std::unordered_set<std::string> allowedVariables;
    auto collectVariables = [&](auto&& self, const ExpressionPtr& expr) -> void {
        if (expr->exprType == Expression::Type::VARIABLE) {
            auto& symbol = std::static_pointer_cast<VariableTerm>(expr)->symbol;
            allowedVariables.insert(symbol);
        }
        else {
            size_t childCount = expr->getChildCount();
            for (size_t i = 0; i < childCount; ++i) {
                self(self, expr->getChild(i));
            }
        }
    };
    if (positiveLiteral) {
        collectVariables(collectVariables, positiveLiteral);
    }

    auto checkVariables = [&](auto&& self, const ExpressionPtr& expr) -> bool {
        if (expr->exprType == Expression::Type::VARIABLE) {
            auto& symbol = std::static_pointer_cast<VariableTerm>(expr)->symbol;
            return allowedVariables.count(symbol) > 0;
        }
        size_t childCount = expr->getChildCount();
        for (size_t i = 0; i < childCount; ++i) {
            if (!self(self, expr->getChild(i))) return false;
        }
        return true;
    };
    for (const auto& literal : literals) {
        if (literal->exprType == Expression::Type::NEGATION) {
            if (!checkVariables(checkVariables, literal)) {
                return selectComplex(literals);
            }
        }
    }

    return selectNothing(literals);
}

//------------------------------------------------------------------------------

Clause::Clause(Literals literals, ProofNodePtr input) :
    origin(Origin::INPUT),
    literals(std::move(literals)),
    input(std::move(input)),
    parent1(nullptr),
    parent2(nullptr) {
}

Clause::Clause(Literals literals, std::string rule, bool simplification,
    ClausePtr parent1, ClausePtr parent2) :
    origin(simplification ? Origin::SIMPLIFICATION : Origin::INFERENCE),
    literals(std::move(literals)),
    input(nullptr),
    rule(std::move(rule)),
    parent1(std::move(parent1)),
    parent2(std::move(parent2)) {
}

ClausePtr Clause::create(Literals literals, ProofNodePtr input) {
    return ClausePtr(new Clause(std::move(literals), std::move(input)));
}

ClausePtr Clause::create(Literals literals, std::string rule, bool simplification,
    ClausePtr parent1, ClausePtr parent2) {
    ClausePtr clause(new Clause(std::move(literals),
        std::move(rule), simplification, std::move(parent1), std::move(parent2)));
    if (clause->parent1) {
        clause->parent1->children.push_back(clause);
    }
    if (clause->parent2 && clause->parent2 != clause->parent1) {
        clause->parent2->children.push_back(clause);
    }
    return clause;
}

Mask Clause::getSelectedLiteralsMask(
    const LiteralSelector& selector) {
    if (selectedLiteralsMask.empty()) {
        selectedLiteralsMask = selector.selectLiterals(literals);
    }
    return selectedLiteralsMask;
}

Mask Clause::getEligibleForResolutionMask(
    const LiteralSelector& selector) {
    if (eligibleForResolutionMask.empty()) {
        eligibleForResolutionMask = selector.areEligibleForResolution(
            literals, getSelectedLiteralsMask(selector));
    }
    return eligibleForResolutionMask;
}

Mask Clause::getEligibleForParamodulationMask(
    const LiteralSelector& selector, bool strictlyMaximal) {
    auto& mask = strictlyMaximal ?
        eligibleForParamodulationSMMask :
        eligibleForParamodulationMask;
    if (mask.empty()) {
        mask = selector.areEligibleForParamodulation(
            literals, getSelectedLiteralsMask(selector), strictlyMaximal);
    }
    return mask;
}

//------------------------------------------------------------------------------

ClauseSelector::ClauseSelector(const std::vector<SelectionStrategy>& strategies,
    const Lpo& lpo) : lpo(lpo) {
    assert(!strategies.empty() && "Must provide at least one selection strategy");
    queues.reserve(strategies.size());
    for (const auto& strategy : strategies) {
        assert(strategy.quota > 0 && "Quota must be greater than 0");
        queues.push_back({ PriorityQueue(), strategy.quota, strategy.evaluator });
    }
}

bool ClauseSelector::isEmpty() const {
    return clauses.empty();
}

void ClauseSelector::addClause(const ClausePtr& clause) {
    if (!clauses.insert(clause).second) return;
    uint64_t id = ++clauseIdCounter;
    for (auto& queue : queues) {
        float weight = queue.evaluator(clause, id, lpo);
        queue.queue.emplace(weight, id, clause);
    }
}

ClausePtr ClauseSelector::selectClause() {
    if (clauses.empty()) return nullptr;
    while (true) {
        StrategyQueue& currentQueue = queues[currentQueueIndex];
        if (currentQueue.queue.empty() || quotaUsed >= currentQueue.quota) {
            currentQueueIndex = (currentQueueIndex + 1) % queues.size();
            quotaUsed = 0;
        }
        else {
            auto [weight, id, clause] = currentQueue.queue.top();
            currentQueue.queue.pop();
            if (clauses.erase(clause)) {
                quotaUsed++;
                return clause;
            }
        }
    }
}

bool ClauseSelector::removeClause(const ClausePtr& clause) {
    return clauses.erase(clause);
}

ClauseSelector::WeightEvaluator ClauseSelector::createFifoWeightEvaluator() {
    return [](const ClausePtr&, uint64_t id, const Lpo&) -> float {
        return static_cast<float>(id);
    };
}

ClauseSelector::WeightEvaluator ClauseSelector::createClauseWeightEvaluator(const std::string& ignoredPredicate) {
    return [ignoredPredicate](const ClausePtr& clause, uint64_t, const Lpo&) -> float {
        constexpr float PREDICATE_WEIGHT = 2.0f;
        constexpr float POS_LITERAL_COEF = 1.0f;

        float totalWeight = 0.0f;
        for (const auto& literal : clause->literals) {
            bool isPositive;
            ExpressionPtr atom;
            if (literal->exprType == Expression::Type::NEGATION) {
                isPositive = false;
                atom = std::static_pointer_cast<NegationFormula>(literal)->child;
            }
            else {
                isPositive = true;
                atom = literal;
            }

            if (!ignoredPredicate.empty() && atom->exprType == Expression::Type::PREDICATE) {
                auto predicate = std::static_pointer_cast<PredicateFormula>(atom);
                if (predicate->symbol == ignoredPredicate) continue;
            }

            float literalWeight = PREDICATE_WEIGHT;
            size_t childCount = atom->getChildCount();
            for (size_t i = 0; i < childCount; ++i) {
                literalWeight += getExpressionWeight(atom->getChild(i));
            }
            if (isPositive) literalWeight *= POS_LITERAL_COEF;
            totalWeight += literalWeight;
        }
        return totalWeight;
    };
}

ClauseSelector::WeightEvaluator ClauseSelector::createRefinedWeightEvaluator(const std::string& ignoredPredicate) {
    return [ignoredPredicate](const ClausePtr& clause, uint64_t, const Lpo& lpo) -> float {
        constexpr float PREDICATE_WEIGHT = 2.0f;
        constexpr float MAX_TERM_COEF = 1.5f;
        constexpr float MAX_LITERAL_COEF = 1.5f;

        const auto& literals = clause->literals;
        size_t literalCount = literals.size();

        // Alternatively, eligible masks can be used
        Mask isLiteralMaximal(literalCount, true);
        for (size_t i = 0; i < literalCount; ++i) {
            for (size_t j = 0; j < literalCount; ++j) {
                if (i != j && isLiteralMaximal[j]) {
                    if (lpo.isGreater(literals[j], literals[i])) {
                        isLiteralMaximal[i] = false;
                        break;
                    }
                }
            }
        }

        float totalClauseWeight = 0.0f;
        for (size_t i = 0; i < literalCount; ++i) {
            auto literal = literals[i];
            ExpressionPtr atom;
            if (literal->exprType == Expression::Type::NEGATION) {
                atom = std::static_pointer_cast<NegationFormula>(literal)->child;
            }
            else atom = literal;

            if (!ignoredPredicate.empty() && atom->exprType == Expression::Type::PREDICATE) {
                auto predicate = std::static_pointer_cast<PredicateFormula>(atom);
                if (predicate->symbol == ignoredPredicate) continue;
            }

            float currentLiteralWeight = PREDICATE_WEIGHT;
            size_t argumentCount = atom->getChildCount();

            if (!isLiteralMaximal[i]) {
                for (size_t j = 0; j < argumentCount; ++j) {
                    currentLiteralWeight += getExpressionWeight(atom->getChild(j));
                }
            }
            else {
                Mask isArgumentMaximal(argumentCount, true);
                for (size_t j = 0; j < argumentCount; ++j) {
                    for (size_t k = 0; k < argumentCount; ++k) {
                        if (j != k && isArgumentMaximal[k]) {
                            if (lpo.isGreater(atom->getChild(k), atom->getChild(j))) {
                                isArgumentMaximal[j] = false;
                                break;
                            }
                        }
                    }
                    auto argumentWeight = getExpressionWeight(atom->getChild(j));
                    if (isArgumentMaximal[j]) argumentWeight *= MAX_TERM_COEF;
                    currentLiteralWeight += argumentWeight;
                }
                currentLiteralWeight *= MAX_LITERAL_COEF;
            }
            totalClauseWeight += currentLiteralWeight;
        }
        return totalClauseWeight;
    };
}

//------------------------------------------------------------------------------

float getExpressionWeight(const ExpressionPtr& expr,
                          float predicateWeight,
                          float functionWeight,
                          float variableWeight) {
    float weight = 0.0f;
    if (expr->exprType == Expression::Type::PREDICATE ||
        expr->exprType == Expression::Type::EQUALITY) {
        weight = predicateWeight;
    }
    else if (expr->exprType == Expression::Type::FUNCTION) {
        weight = functionWeight;
    }
    else if (expr->exprType == Expression::Type::VARIABLE) {
        weight = variableWeight;
    }
    size_t childCount = expr->getChildCount();
    for (size_t i = 0; i < childCount; ++i) {
        weight += getExpressionWeight(expr->getChild(i));
    }
    return weight;
}

} // namespace SuperpositionSolverUtils

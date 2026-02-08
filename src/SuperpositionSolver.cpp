#include "SuperpositionSolver.hpp"

#include "ExpressionUtils.hpp"
#include "Unification.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_set>

using namespace Unification;

struct SuperpositionSolver::Clause {
    const ProofNodePtr input;

    const std::string rule;
    const ClausePtr parent1;
    const ClausePtr parent2;

    std::vector<FormulaPtr> literals;

private:
    std::vector<std::weak_ptr<Clause>> children;
    bool redundant = false;

    Clause(ProofNodePtr input) :
        input(input), parent1(nullptr), parent2(nullptr) {}
    Clause(std::string rule, ClausePtr parent1, ClausePtr parent2 = nullptr) :
        rule(std::move(rule)), input(nullptr), parent1(parent1), parent2(parent2) {}

public:
    static ClausePtr create(ProofNodePtr input) {
        return ClausePtr(new Clause(input));
    }

    static ClausePtr create(std::string rule, ClausePtr parent1, ClausePtr parent2 = nullptr) {
        ClausePtr clause(new Clause(std::move(rule), parent1, parent2));
        if (clause->parent1) clause->parent1->children.push_back(clause);
        if (clause->parent2 && clause->parent2 != clause->parent1) {
            clause->parent2->children.push_back(clause);
        }
        return clause;
    }

    bool isRedundant() const { return redundant; }

    void markChildrenRedundant(const ClausePtr& exempt = nullptr) {
        auto it = children.begin();
        while (it != children.end()) {
            if (auto childPtr = it->lock()) {
                if (childPtr != exempt) {
                    childPtr->redundant = true;
                }
                ++it;
            }
            else {
                it = children.erase(it);
            }
        }
    }
};

class SuperpositionSolver::ClauseSelector {
public:
    using WeightEvaluator = std::function<float(const ClausePtr& clause, uint64_t id)>;
    struct SelectionStrategy {
        size_t quota;
        WeightEvaluator evaluator;
    };

    explicit ClauseSelector(const std::vector<SelectionStrategy>& strategies);

    bool isEmpty() const;
    void addClause(const ClausePtr& clause);
    ClausePtr selectClause();
    bool removeClause(const ClausePtr& clause);

private:
    using QueueElement = std::tuple<float, uint64_t, ClausePtr>;
    using PriorityQueue = std::priority_queue
        <QueueElement, std::vector<QueueElement>, std::greater<QueueElement>>;

    struct StrategyQueue {
        PriorityQueue queue;
        size_t quota;
        WeightEvaluator evaluator;
    };

    std::vector<StrategyQueue> queues;
    std::unordered_set<ClausePtr> clauses;

    uint64_t clauseIdCounter = 0;
    size_t currentQueueIndex = 0;
    size_t quotaUsed = 0;
};

SuperpositionSolver::ClauseSelector::ClauseSelector(
    const std::vector<SelectionStrategy>& strategies)
{
    assert(!strategies.empty() && "Must provide at least one selection strategy");
    queues.reserve(strategies.size());
    for (const auto& strategy : strategies) {
        assert(strategy.quota > 0 && "Quota must be greater than 0");
        queues.push_back({ PriorityQueue(), strategy.quota, strategy.evaluator });
    }
}

bool SuperpositionSolver::ClauseSelector::isEmpty() const {
    return clauses.empty();
}

void SuperpositionSolver::ClauseSelector::addClause(const ClausePtr& clause) {
    if (!clauses.insert(clause).second) return;
    uint64_t id = ++clauseIdCounter;
    for (auto& queue : queues) {
        float weight = queue.evaluator(clause, id);
        queue.queue.emplace(weight, id, clause);
    }
}

SuperpositionSolver::ClausePtr SuperpositionSolver::ClauseSelector::selectClause() {
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

bool SuperpositionSolver::ClauseSelector::removeClause(const ClausePtr& clause) {
    return clauses.erase(clause);
}

class SuperpositionSolver::ClauseIndex {
public:
    bool isEmpty() const {
        return clauses.empty();
    }

    void addClause(const ClausePtr& clause) {
        clauses.push_back(clause);
        if (clause->literals.size() == 1) unitClauses.push_back(clause);
    }

    bool removeClause(const ClausePtr& clause) {
        if (clause->literals.size() == 1) {
            for (auto& c : unitClauses) {
                if (c == clause) {
                    c = unitClauses.back();
                    unitClauses.pop_back();
                    break;
                }
            }
        }
        for (auto& c : clauses) {
            if (c == clause) {
                c = clauses.back();
                clauses.pop_back();
                return true;
            }
        }
        return false;
    }

    const Clauses& getClauses() const {
        return clauses;
    }

    const Clauses& getUnitClauses() const {
        return unitClauses;
    }

    // --- 1. Generowanie Wnioskow (INFERENCE) ---
    // Te metody wykorzystuja UNIFIKACJE (Unification).

    // Znajduje klauzule, ktore posiadaja literal unifikowalny z podanym 'literal'.
    // Uzywane w kroku Generating (Binary Resolution).
    // Zazwyczaj szukamy literalu o przeciwnej polaryzacji.
    Clauses getUnifiableClauses(const FormulaPtr& literal) {
        return {}; // Placeholder
    }

    // Znajduje klauzule, ktore zawieraja podterm unifikowalny z podanym termem.
    // Uzywane w kroku Generating (Superposition).
    // 'term' to zazwyczaj jedna ze stron rownania L=R z nowej klauzuli.
    Clauses getUnifiableWithTerm(const TermPtr& term) {
        return {}; // Placeholder
    }

    // --- 2. Upraszczanie Nowej Klauzuli (FORWARD SIMPLIFICATION) ---
    // Te metody wykorzystuja DOPASOWANIE (Matching).

    // Znajduje w indeksie klauzule bedace rownosciami (Unit Equality L=R),
    // gdzie L pasuje (matches) do jakiegokolwiek podtermu w 'term'.
    // Uzywane do Demodulacji (przepisywania) nowej klauzuli wiedza zebrana wczesniej.
    Clauses getRewritingRulesFor(const TermPtr& term) {
        return {}; // Placeholder
    }

    // Znajduje w indeksie klauzule, ktora subsumuje (czyni zbedna) podana 'clause'.
    // Jesli zwroci jakikolwiek wynik, 'clause' jest redundantna i mozna ja usunac.
    // Subsumption: Clause A subsumes B if A sigma subset B.
    Clauses getSubsumingClauses(const ClausePtr& clause) {
        return {}; // Placeholder
    }

    // --- 3. Upraszczanie Starych Klauzul (BACKWARD SIMPLIFICATION) ---
    // Te metody rowniez wykorzystuja DOPASOWANIE, ale w druga strone.

    // Znajduje w indeksie stare klauzule, ktore zawieraja podterm,
    // ktory moze zostac przepisany przez nowa regule (equationLHS = ...).
    // Znalezione klauzule zostana usuniete z indeksu, uproszczone i dodane ponownie do Unprocessed.
    Clauses getClausesRewritableBy(const TermPtr& equationLHS) {
        return {}; // Placeholder
    }

    // Znajduje w indeksie stare klauzule, ktore sa subsumowane przez nowa 'clause'.
    // Te stare klauzule staja sie redundantne i nalezy je usunac z indeksu.
    Clauses getSubsumedClauses(const ClausePtr& clause) {
        return {}; // Placeholder
    }

private:
    Clauses clauses;
    Clauses unitClauses;
};

void SuperpositionSolver::setTimeLimit(int seconds) {
    timeLimitSeconds = static_cast<double>(seconds);
}

void SuperpositionSolver::setMemoryLimit(int megabytes) {
    memoryLimitMegabytes = megabytes;
}

FolSatSolver::Result SuperpositionSolver::solve(const std::vector<ProofNodePtr>& clauses) {
    auto startTime = std::chrono::steady_clock::now();
    size_t iterationCounter = 0;
    auto checkLimits = [&]() -> FolSatSolver::Result {
        if (++iterationCounter > 128) {
            iterationCounter = 0;
            if (timeLimitSeconds > 0.0) {
                auto now = std::chrono::steady_clock::now();
                std::chrono::duration<double> elapsed = now - startTime;
                if (elapsed.count() > timeLimitSeconds) {
                    return FolSatSolver::Result::TIME_OUT;
                }
            }
            if (memoryLimitMegabytes > 0) {
                auto memoryLimitBytes = memoryLimitMegabytes * 1024 * 1024;
                if (getPeakMemoryUsageInBytes() > memoryLimitBytes) {
                    return FolSatSolver::Result::MEMORY_OUT;
                }
            }
        }
        return FolSatSolver::Result::UNKNOWN;
    };

    ClauseSelector unprocessedClauses = createClauseSelector();
    ClauseIndex processedClauses;
    transformer = ExpressionTransformer();
    proofRoot = nullptr;

    auto contradiction = loadInitialClauses(clauses, unprocessedClauses);
    if (contradiction) return FolSatSolver::Result::UNSATISFIABLE;

    while (!unprocessedClauses.isEmpty()) {
        if (auto result = checkLimits(); result != FolSatSolver::Result::UNKNOWN) return result;

        ClausePtr givenClause = unprocessedClauses.selectClause();
        if (!givenClause || givenClause->isRedundant()) continue;

        givenClause = simplifyForward(givenClause, processedClauses);
        if (!givenClause) continue;
        if (givenClause->literals.empty()) {
            proofRoot = givenClause;
            return FolSatSolver::Result::UNSATISFIABLE;
        }

        Clauses derivedClauses;
        simplifyBackward(processedClauses, givenClause, derivedClauses);
        generateInferences(givenClause, processedClauses, derivedClauses);
        processedClauses.addClause(givenClause);

        for (auto clause : derivedClauses) {
            if (!clause) continue;
            clause = simplifyCheapForward(clause, processedClauses);
            if (!clause) continue;
            clause = simplifyNecessary(clause);
            if (!clause) continue;
            if (clause->literals.empty()) {
                proofRoot = clause;
                return FolSatSolver::Result::UNSATISFIABLE;
            }
            standardizeVariables(clause);
            unprocessedClauses.addClause(clause);
        }
    }
    return FolSatSolver::Result::SATISFIABLE;
}

ProofNodePtr SuperpositionSolver::getProof() const {
    if (!proofRoot) return nullptr;
    std::map<ClausePtr, ProofNodePtr> cache;
    return reconstructProof(proofRoot, cache);
}

SuperpositionSolver::ClauseSelector SuperpositionSolver::createClauseSelector() const {
    auto fifoWeightEvaluator = [](const ClausePtr&, uint64_t id) -> float {
        return static_cast<float>(id);
    };

    auto clauseWeightEvaluator = [](const ClausePtr& clause, uint64_t) -> float {
        constexpr float PREDICATE_WEIGHT = 2.0f;
        constexpr float FUNCTION_WEIGHT = 2.0f;
        constexpr float VARIABLE_WEIGHT = 1.0f;
        constexpr float POS_LITERAL_COEF = 1.0f;

        std::function<float(const ExpressionPtr&)> getTermWeight =
            [&](const ExpressionPtr& expr) -> float {
            if (expr->exprType == Expression::Type::VARIABLE) return VARIABLE_WEIGHT;
            assert(expr->exprType == Expression::Type::FUNCTION);
            float weight = FUNCTION_WEIGHT;
            size_t childCount = expr->getChildCount();
            for (size_t i = 0; i < childCount; ++i) {
                weight += getTermWeight(expr->getChild(i));
            }
            return weight;
        };

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
            float literalWeight = PREDICATE_WEIGHT;
            size_t childCount = atom->getChildCount();
            for (size_t i = 0; i < childCount; ++i) {
                literalWeight += getTermWeight(atom->getChild(i));
            }
            if (isPositive) literalWeight *= POS_LITERAL_COEF;
            totalWeight += literalWeight;
        }
        return totalWeight;
    };

    auto refinedWeightEvaluator = [this](const ClausePtr& clause, uint64_t) -> float {
        constexpr float PREDICATE_WEIGHT = 2.0f;
        constexpr float FUNCTION_WEIGHT = 2.0f;
        constexpr float VARIABLE_WEIGHT = 1.0f;
        constexpr float MAX_TERM_COEF = 1.5f;
        constexpr float MAX_LITERAL_COEF = 1.5f;

        std::function<float(const ExpressionPtr&)> getBaseTermWeight =
            [&](const ExpressionPtr& expr) -> float {
            if (expr->exprType == Expression::Type::VARIABLE) return VARIABLE_WEIGHT;
            assert(expr->exprType == Expression::Type::FUNCTION);
            float weight = FUNCTION_WEIGHT;
            size_t childCount = expr->getChildCount();
            for (size_t i = 0; i < childCount; ++i) {
                weight += getBaseTermWeight(expr->getChild(i));
            }
            return weight;
        };

        const auto& literals = clause->literals;
        size_t literalCount = literals.size();

        // Alternatively, eligible masks can be used
        std::vector<bool> isLiteralMaximal(literalCount, true);
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

            float currentLiteralWeight = PREDICATE_WEIGHT;
            size_t argumentCount = atom->getChildCount();

            if (!isLiteralMaximal[i]) {
                for (size_t j = 0; j < argumentCount; ++j) {
                    currentLiteralWeight += getBaseTermWeight(atom->getChild(j));
                }
            }
            else {
                std::vector<bool> isArgumentMaximal(argumentCount, true);
                for (size_t j = 0; j < argumentCount; ++j) {
                    for (size_t k = 0; k < argumentCount; ++k) {
                        if (j != k && isArgumentMaximal[k]) {
                            if (lpo.isGreater(atom->getChild(k), atom->getChild(j))) {
                                isArgumentMaximal[j] = false;
                                break;
                            }
                        }
                    }
                    auto argumentWeight = getBaseTermWeight(atom->getChild(j));
                    if (isArgumentMaximal[j]) argumentWeight *= MAX_TERM_COEF;
                    currentLiteralWeight += argumentWeight;
                }
                currentLiteralWeight *= MAX_LITERAL_COEF;
            }
            totalClauseWeight += currentLiteralWeight;
        }
        return totalClauseWeight;
    };

    std::vector<ClauseSelector::SelectionStrategy> selectionStrategies = {
        { 3, refinedWeightEvaluator },
        { 1, clauseWeightEvaluator },
        { 1, fifoWeightEvaluator }
    };
    return ClauseSelector(selectionStrategies);
}

bool SuperpositionSolver::loadInitialClauses(const std::vector<ProofNodePtr>& clauses,
    ClauseSelector& unprocessedClauses) {
    for (size_t i = 0; i < clauses.size(); ++i) {
        const auto& clause = clauses[i]->getFormula();
        assert(clause);
        assert(clause->exprType == Expression::Type::JUNCTION);
        auto junction = std::static_pointer_cast<JunctionFormula>(clause->clone());
        ClausePtr inputClause = Clause::create(clauses[i]);
        inputClause->literals = junction->operands;

        std::vector<FormulaPtr> workingLiterals = junction->operands;
        bool changed = false;
        bool isTautology = removeBoolLiterals(workingLiterals, &changed);
        if (isTautology) continue;
        isTautology = handleDistinctObjects(workingLiterals, &changed);
        if (isTautology) continue;
        ClausePtr finalClause = inputClause;
        if (changed) {
            finalClause = Clause::create("simplification", inputClause);
            finalClause->literals = std::move(workingLiterals);
        }
        if (finalClause->literals.empty()) {
            proofRoot = finalClause;
            return true;
        }
        standardizeVariables(finalClause);
        unprocessedClauses.addClause(finalClause);
    }
    return false;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::simplifyForward(
    const ClausePtr& clauseToSimplify, const ClauseIndex& index) const {
    // (RN), (RP), (NS), (PS), (CLC), (DR), (DD), (DE), (CS), (ES), (TD)

    auto current = clauseToSimplify;
    bool changed;

    do {
        changed = false;

        auto afterTD = applyTautologyDeletion(current);
        if(afterTD == nullptr) return nullptr;

        auto afterDE = applyDestructiveEqualityResolution(current);
        if (afterDE != current) {
            current = afterDE;
            changed = true;
        }

        auto afterDR = applyDeletionOfResolvedLiterals(current);
        if (afterDR != current) {
            current = afterDR;
            changed = true;
        }

        auto afterDD = applyDeletionOfDuplicateLiterals(current);
        if (afterDD != current) {
            current = afterDD;
            changed = true;
        }

        for (const auto& unitClause : index.getUnitClauses()) {
            auto afterPUS = applyPredicateUnitSimplification(current, unitClause);
            if (afterPUS != current) {
                current = afterPUS;
                changed = true;
                if (!current) return nullptr;
            }
        }

        for (const auto& unitClause : index.getUnitClauses()) {
            auto afterDemodulation = applyDemodulation(current, unitClause);
            if (afterDemodulation != current) {
                current = afterDemodulation;
                changed = true;
            }
        }

        for (const auto& unitClause : index.getUnitClauses()) {
            auto afterPS = applyPositiveSimplifyReflect(current, unitClause);
            if (afterPS != current) {
                current = afterPS;
                changed = true;
            }
        }

        for (const auto& unitClause : index.getUnitClauses()) {
            auto afterNS = applyNegativeSimplifyReflect(current, unitClause);
            if (afterNS != current) {
                current = afterNS;
                changed = true;
            }
        }

        for (const auto& procClause : index.getClauses()) {
            if (!applyClauseSubsumption(current, procClause)) return nullptr;
        }

        for (const auto& unitClause : index.getUnitClauses()) {
            if (!applyEqualitySubsumption(current, unitClause)) return nullptr;
        }
    } while (changed);

    return current;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::simplifyCheapForward(
    const ClausePtr& clauseToSimplify, const ClauseIndex& index) const {
    // efficiently implemented subset of (RN), (RP), (NS), (PS), (CLC), (DR), (DD), (DE), (TD)
    return applyTautologyDeletion(clauseToSimplify);
}

void SuperpositionSolver::simplifyBackward(
    ClauseIndex& indexToSimplify, const ClausePtr& clause, Clauses& reducedClauses) const {

    auto deleteAndReplace = [&](const ClausePtr& oldClause, const ClausePtr& newClause = nullptr) {
        assert(oldClause);
        if (oldClause == newClause) return;
        oldClause->markChildrenRedundant(newClause);
        indexToSimplify.removeClause(oldClause);
        if (newClause) reducedClauses.push_back(newClause);
    };

    if (clause->literals.size() == 1) {
        auto procClauses = indexToSimplify.getClauses();
        for (auto& procClause : procClauses) {
            auto result = applyPredicateUnitSimplification(procClause, clause);
            deleteAndReplace(procClause, result);
        }
    }

    auto procClauses = indexToSimplify.getClauses();
    for (const auto& procClause : procClauses) {
        auto result = applyClauseSubsumption(procClause, clause);
        deleteAndReplace(procClause, result);
    }

    if (clause->literals.size() == 1) {
        auto procClauses = indexToSimplify.getClauses();
        for (const auto& procClause : procClauses) {
            auto result = applyEqualitySubsumption(procClause, clause);
            deleteAndReplace(procClause, result);
        }
    }

    if (clause->literals.size() == 1) {
        auto procClauses = indexToSimplify.getClauses();
        for (auto& procClause : procClauses) {
            auto result = applyDemodulation(procClause, clause);
            deleteAndReplace(procClause, result);
        }
    }

    if (clause->literals.size() == 1) {
        auto procClauses = indexToSimplify.getClauses();
        for (auto& procClause : procClauses) {
            auto result = applyPositiveSimplifyReflect(procClause, clause);
            deleteAndReplace(procClause, result);
        }
    }

    if (clause->literals.size() == 1) {
        auto procClauses = indexToSimplify.getClauses();
        for (auto& procClause : procClauses) {
            auto result = applyNegativeSimplifyReflect(procClause, clause);
            deleteAndReplace(procClause, result);
        }
    }
}

SuperpositionSolver::ClausePtr SuperpositionSolver::simplifyNecessary(
    const ClausePtr& clauseToSimplify) const {
    std::vector<FormulaPtr> workingLiterals = clauseToSimplify->literals;
    bool changed = false;
    auto isTautology = handleDistinctObjects(workingLiterals, &changed);
    if (isTautology) return nullptr;
    if (changed) {
        auto finalClause = Clause::create("simplification", clauseToSimplify);
        finalClause->literals = std::move(workingLiterals);
        return finalClause;
    }
    return clauseToSimplify;
}

void SuperpositionSolver::generateInferences(
    const ClausePtr& clause, const ClauseIndex& index, Clauses& inferredClauses) const {
    applyFactoring(clause, inferredClauses);
    applyEqualityResolution(clause, inferredClauses);
    applyEqualityFactoring(clause, inferredClauses);
    for (const auto& procClause : index.getClauses()) {
        applyBinaryResolution(procClause, clause, inferredClauses);
        applySuperposition(procClause, clause, inferredClauses);
    }
}

void SuperpositionSolver::applyBinaryResolution(
    const ClausePtr& clause1, const ClausePtr& clause2,
    Clauses& resolvents) const {
    if (clause1 == clause2) return;

    auto resolve = [&](const ClausePtr& lClause, const std::vector<bool>& lSelection,
                       const ClausePtr& rClause, const std::vector<bool>& rSelection) {
        auto lEligibleMask = areEligibleForParamodulation(lClause->literals, lSelection, true);
        auto rEligibleMask = areEligibleForResolution(rClause->literals, rSelection);

        for (size_t i = 0; i < lClause->literals.size(); ++i) {
            if (!lEligibleMask[i]) continue;
            auto lLiteral = lClause->literals[i];
            if (lLiteral->exprType == Expression::Type::NEGATION) continue;

            for (size_t j = 0; j < rClause->literals.size(); ++j) {
                if (!rEligibleMask[j]) continue;
                auto rLiteral = rClause->literals[j];
                if (rLiteral->exprType != Expression::Type::NEGATION) continue;
                auto negation = std::static_pointer_cast<NegationFormula>(rLiteral);

                Substitution mgu;
                if (unify(lLiteral, negation->child, mgu)) {
                    auto rule = "resolution";
                    auto newClause = Clause::create(rule, lClause, rClause);
                    for (size_t k = 0; k < lClause->literals.size(); ++k) {
                        if (k != i) {
                            auto newLiteral = substitute(lClause->literals[k], mgu);
                            newClause->literals.push_back(
                                std::static_pointer_cast<Formula>(newLiteral));
                        }
                    }
                    for (size_t k = 0; k < rClause->literals.size(); ++k) {
                        if (k != j) {
                            auto newLiteral = substitute(rClause->literals[k], mgu);
                            newClause->literals.push_back(
                                std::static_pointer_cast<Formula>(newLiteral));
                        }
                    }
                    resolvents.push_back(newClause);
                }
            }
        }
   };

    auto selectionMask1 = selectLiterals(clause1->literals);
    auto selectionMask2 = selectLiterals(clause2->literals);
    resolve(clause1, selectionMask1, clause2, selectionMask2);
    resolve(clause2, selectionMask2, clause1, selectionMask1);
}

void SuperpositionSolver::applyFactoring(
    const ClausePtr& clause, Clauses& factors) const {
    size_t literalCount = clause->literals.size();
    if (literalCount < 2) return;

    auto selectionMask = selectLiterals(clause->literals);
    auto eligibleMask = areEligibleForParamodulation(clause->literals, selectionMask);

    for (size_t i = 0; i < literalCount; ++i) {
        if (!eligibleMask[i]) continue;
        auto literal1 = clause->literals[i];
        if (literal1->exprType != Expression::Type::PREDICATE) continue;

        for (size_t j = 0; j < literalCount; ++j) {
            if (i == j) continue;
            auto literal2 = clause->literals[j];
            if (literal2->exprType != Expression::Type::PREDICATE) continue;
            if (eligibleMask[j] && j < i) continue;

            Substitution mgu;
            if (unify(literal1, literal2, mgu)) {
                auto rule = "factoring";
                auto newClause = Clause::create(rule, clause);
                for (size_t k = 0; k < literalCount; ++k) {
                    if (k != i) {
                        auto newLiteral = substitute(clause->literals[k], mgu);
                        newClause->literals.push_back(
                            std::static_pointer_cast<Formula>(newLiteral)
                        );
                    }
                }
                factors.push_back(newClause);
            }
        }
    }
}

void SuperpositionSolver::applySuperposition(
    const ClausePtr& clause1, const ClausePtr& clause2,
    Clauses& paramodulants) const {
    if (clause1 == clause2) return;

    std::function<void(ExpressionPtr, std::vector<size_t>&,
        const ClausePtr&, const ClausePtr&, size_t, size_t,
        const TermPtr&, const TermPtr&)> matchAndRewriteSubterms = [&](
            ExpressionPtr expression, std::vector<size_t>& path,
            const ClausePtr& fromClause, const ClausePtr& intoClause,
            size_t fromLiteralIndex, size_t intoLiteralIndex,
            const TermPtr& patternTerm, const TermPtr& replacementTerm) {

        if (expression->isTerm() && expression->exprType != Expression::Type::VARIABLE) {
            Substitution mgu;
            if (unify(patternTerm, expression, mgu)) {
                auto rule = "superposition";
                auto newClause = Clause::create(rule, fromClause, intoClause);
                for (size_t i = 0; i < fromClause->literals.size(); ++i) {
                    if (i != fromLiteralIndex) {
                        auto literal = std::static_pointer_cast<Formula>(substitute(fromClause->literals[i], mgu));
                        newClause->literals.push_back(literal);
                    }
                }
                for (size_t i = 0; i < intoClause->literals.size(); ++i) {
                    if (i != intoLiteralIndex) {
                        auto literal = std::static_pointer_cast<Formula>(substitute(intoClause->literals[i], mgu));
                        newClause->literals.push_back(literal);
                    }
                    else {
                        auto intoLiteralClone = intoClause->literals[i]->clone();
                        ExpressionPtr parent = intoLiteralClone;
                        assert(!path.empty());
                        for (size_t depth = 0; depth < path.size() - 1; ++depth) {
                            parent = parent->getChild(path[depth]);
                        }
                        parent->setChild(path.back(), replacementTerm->clone());
                        auto literal = std::static_pointer_cast<Formula>(substitute(intoLiteralClone, mgu, true));
                        newClause->literals.push_back(literal);
                    }
                }
                paramodulants.push_back(newClause);
            }
        }

        size_t childCount = expression->getChildCount();
        for (size_t i = 0; i < childCount; ++i) {
            path.push_back(i);
            matchAndRewriteSubterms(
                expression->getChild(i), path,
                fromClause, intoClause,
                fromLiteralIndex, intoLiteralIndex,
                patternTerm, replacementTerm);
            path.pop_back();
        }
    };

    auto processClausePair = [this, matchAndRewriteSubterms](
        const ClausePtr& fromClause, const std::vector<bool>& fromSelMask,
        const ClausePtr& intoClause, const std::vector<bool>& intoSelMask) {

        auto fromEligibleMask = areEligibleForParamodulation(fromClause->literals, fromSelMask, true);
        auto intoEligibleMask = areEligibleForResolution(intoClause->literals, intoSelMask);

        for (size_t i = 0; i < fromClause->literals.size(); ++i) {
            if (!fromEligibleMask[i]) continue;
            auto fromLiteral = fromClause->literals[i];
            if (fromLiteral->exprType != Expression::Type::EQUALITY) continue;
            auto equality = std::static_pointer_cast<EqualityFormula>(fromLiteral);
            auto cmpResult = lpo.compare(equality->left, equality->right);

            for (size_t j = 0; j < intoClause->literals.size(); ++j) {
                if (!intoEligibleMask[j]) continue;
                auto intoLiteral = intoClause->literals[j];

                auto processLiteralPair = [&](const TermPtr& pattern, const TermPtr& replacement) {
                    ExpressionPtr current = intoLiteral;
                    std::vector<size_t> path;
                    if (current->exprType == Expression::Type::NEGATION) {
                        path.push_back(0);
                        current = std::static_pointer_cast<NegationFormula>(current)->child;
                    }
                    if (current->exprType == Expression::Type::EQUALITY) {
                        auto intoEquality = std::static_pointer_cast<EqualityFormula>(current);
                        auto cmpResult = lpo.compare(intoEquality->left, intoEquality->right);
                        if (cmpResult != Lpo::Result::LESS) {
                            path.push_back(0);
                            matchAndRewriteSubterms(intoEquality->left, path,
                                fromClause, intoClause, i, j, pattern, replacement);
                            path.pop_back();
                        }
                        if (cmpResult != Lpo::Result::GREATER) {
                            path.push_back(1);
                            matchAndRewriteSubterms(intoEquality->right, path,
                                fromClause, intoClause, i, j, pattern, replacement);
                            path.pop_back();
                        }
                    }
                    else {
                        matchAndRewriteSubterms(current, path,
                            fromClause, intoClause, i, j, pattern, replacement);
                    }
                };

                if (cmpResult != Lpo::Result::LESS) {
                    processLiteralPair(equality->left, equality->right);
                }
                if (cmpResult != Lpo::Result::GREATER) {
                    processLiteralPair(equality->right, equality->left);
                }
            }
        }
    };

    auto selectionMask1 = selectLiterals(clause1->literals);
    auto selectionMask2 = selectLiterals(clause2->literals);
    processClausePair(clause1, selectionMask1, clause2, selectionMask2);
    processClausePair(clause2, selectionMask2, clause1, selectionMask1);
}

void SuperpositionSolver::applyEqualityResolution(const ClausePtr& clause,
    Clauses& inferredClauses) const {
    auto selectionMask = selectLiterals(clause->literals);
    auto eligibleMask = areEligibleForResolution(clause->literals, selectionMask);
    for (size_t i = 0; i < clause->literals.size(); ++i) {
        if (eligibleMask[i]) {
            auto literal = clause->literals[i];
            if (literal->exprType != Expression::Type::NEGATION) continue;
            auto negation = std::static_pointer_cast<NegationFormula>(literal);
            if (negation->child->exprType != Expression::Type::EQUALITY) continue;
            auto equality = std::static_pointer_cast<EqualityFormula>(negation->child);
            Substitution mgu;
            if (unify(equality->left, equality->right, mgu)) {
                auto rule = "equality_resolution";
                auto newClause = Clause::create(rule, clause);
                for (size_t j = 0; j < clause->literals.size(); ++j) {
                    if (j != i) {
                        newClause->literals.push_back(
                            std::static_pointer_cast<Formula>(substitute(clause->literals[j], mgu)));
                    }
                }
                inferredClauses.push_back(newClause);
            }
        }
    }
}

void SuperpositionSolver::applyEqualityFactoring(const ClausePtr& clause,
    Clauses& inferredClauses) const {
    size_t literalCount = clause->literals.size();
    if (literalCount < 2) return;

    auto selectionMask = selectLiterals(clause->literals);
    auto eligibleMask = areEligibleForParamodulation(clause->literals, selectionMask);

    for (size_t i = 0; i < literalCount; ++i) {
        if (!eligibleMask[i]) continue;
        auto literal1 = clause->literals[i];
        if (literal1->exprType != Expression::Type::EQUALITY) continue;
        auto equality1 = std::static_pointer_cast<EqualityFormula>(literal1);

        auto processPrimarySide = [&](TermPtr s, TermPtr t) {
            for (size_t j = 0; j < literalCount; ++j) {
                if (i == j) continue;
                auto literal2 = clause->literals[j];
                if (literal2->exprType != Expression::Type::EQUALITY) continue;
                auto equality2 = std::static_pointer_cast<EqualityFormula>(literal2);
 
                auto processPartnerSide = [&](TermPtr u, TermPtr v) {
                    Substitution mgu;
                    if (unify(s, u, mgu)) {
                        auto rule = "equality_factoring";
                        auto newClause = Clause::create(rule, clause);
                        auto tSub = substitute(t, mgu);
                        auto vSub = substitute(v, mgu);
                        auto newEquality = std::make_shared<EqualityFormula>(
                            std::static_pointer_cast<Term>(tSub),
                            std::static_pointer_cast<Term>(vSub));
                        auto newInequality = std::make_shared<NegationFormula>(newEquality);
                        newClause->literals.push_back(newInequality);
                        for (size_t k = 0; k < literalCount; ++k) {
                            if (k != i) {
                                auto newLiteral = substitute(clause->literals[k], mgu);
                                newClause->literals.push_back(
                                    std::static_pointer_cast<Formula>(newLiteral)
                                );
                            }
                        }
                        inferredClauses.push_back(newClause);
                    }
                };

                TermPtr& lhs = equality2->left;
                TermPtr& rhs = equality2->right;
                processPartnerSide(lhs, rhs);
                processPartnerSide(rhs, lhs);
            }
        };

        TermPtr& lhs = equality1->left;
        TermPtr& rhs = equality1->right;
        auto cmp = lpo.compare(lhs, rhs);
        if (cmp == Lpo::Result::GREATER || cmp == Lpo::Result::EQUAL) {
            processPrimarySide(lhs, rhs);
        }
        else if (cmp == Lpo::Result::LESS) {
            processPrimarySide(rhs, lhs);
        }
        else if (cmp == Lpo::Result::INCOMPARABLE) {
            processPrimarySide(lhs, rhs);
            processPrimarySide(rhs, lhs);
        }
    }
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyTautologyDeletion(
    const ClausePtr& clause) const {
    // Syntactic check only - semantic tautologies are not detected here
    if (!clause) return nullptr;

    const auto& literals = clause->literals;
    for (size_t i = 0; i < literals.size(); ++i) {
        if (literals[i]->exprType == Expression::Type::EQUALITY) {
            auto equality = std::static_pointer_cast<EqualityFormula>(literals[i]);
            if (lpo.isEqual(equality->left, equality->right)) return nullptr;
        }
        for (size_t j = 0; j < literals.size(); ++j) {
            if (j == i) continue;
            if (literals[i]->exprType != Expression::Type::NEGATION &&
                literals[j]->exprType == Expression::Type::NEGATION) {
                auto atom = std::static_pointer_cast<NegationFormula>(literals[j])->child;
                if (lpo.isEqual(literals[i], atom)) return nullptr;
            }
        }
    }
    return clause;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyDeletionOfDuplicateLiterals(
    const ClausePtr& clause) const {
    if (!clause) return nullptr;

    std::vector<FormulaPtr> uniqueLiterals;
    uniqueLiterals.reserve(clause->literals.size());

    for (const auto& literal : clause->literals) {
        bool isDuplicate = false;
        for (const auto& existing : uniqueLiterals) {
            if (lpo.isEqual(literal, existing)) {
                isDuplicate = true;
                break;
            }
        }
        if (!isDuplicate) uniqueLiterals.push_back(literal);
    }

    if (uniqueLiterals.size() == clause->literals.size()) return clause;
    auto newClause = Clause::create("deletion_of_duplicate_literals", clause);
    newClause->literals = std::move(uniqueLiterals);
    return newClause;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyDeletionOfResolvedLiterals(
    const ClausePtr& clause) const {
    if (!clause) return nullptr;

    std::vector<FormulaPtr> literals;
    literals.reserve(clause->literals.size());

    for (const auto& literal : clause->literals) {
        bool remove = false;
        if (literal->exprType == Expression::Type::NEGATION) {
            auto negation = std::static_pointer_cast<NegationFormula>(literal);
            if (negation->child->exprType == Expression::Type::EQUALITY) {
                auto equality = std::static_pointer_cast<EqualityFormula>(negation->child);
                if (lpo.isEqual(equality->left, equality->right)) remove = true;
            }
        }
        if (!remove) literals.push_back(literal);
    }

    if (literals.size() == clause->literals.size()) return clause;
    auto newClause = Clause::create("deletion_of_resolved_literals", clause);
    newClause->literals = std::move(literals);
    return newClause;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyDestructiveEqualityResolution(
    const ClausePtr& clause) const {
    if (!clause) return nullptr;

    for (size_t i = 0; i < clause->literals.size(); ++i) {
        const auto& literal = clause->literals[i];
        if (literal->exprType != Expression::Type::NEGATION) continue;
        auto negation = std::static_pointer_cast<NegationFormula>(literal);
        if (negation->child->exprType != Expression::Type::EQUALITY) continue;
        auto equality = std::static_pointer_cast<EqualityFormula>(negation->child);
        if (equality->left->exprType != Expression::Type::VARIABLE ||
            equality->right->exprType != Expression::Type::VARIABLE) {
            continue;
        }

        Substitution mgu;
        if (Unification::unify(equality->left, equality->right, mgu)) {
            auto newClause = Clause::create("destructive_equality_resolution", clause);
            newClause->literals.reserve(clause->literals.size() - 1);
            for (size_t j = 0; j < clause->literals.size(); ++j) {
                if (i == j) continue;
                auto newLiteral = Unification::substitute(clause->literals[j], mgu);
                newClause->literals.push_back(std::static_pointer_cast<Formula>(newLiteral));
            }
            return newClause;
        }
    }
    return clause;
}

// Implements clause simplification based on a predicate unit clause (with or without negation).
// Complements equality-focused rules like Demodulation (RN, RP), Equality Subsumption (ES), and
// Simplify-Reflect (PS, NS) by removing predicate literals that contradict the established unit fact.
SuperpositionSolver::ClausePtr SuperpositionSolver::applyPredicateUnitSimplification(
    const ClausePtr& clause, const ClausePtr& unitClause) const {
    if (!clause) return nullptr;
    assert(unitClause);

    if (unitClause->literals.size() != 1) return clause;
    auto unitLiteral = unitClause->literals.front();
    bool isUnitNegated = (unitLiteral->exprType == Expression::Type::NEGATION);
    auto unitAtom = isUnitNegated ?
        std::static_pointer_cast<NegationFormula>(unitLiteral)->child : unitLiteral;
    if (unitAtom->exprType != Expression::Type::PREDICATE) return clause;

    std::vector<FormulaPtr> newLiterals;
    bool changed = false;
    for (size_t i = 0; i < clause->literals.size(); ++i) {
        auto literal = clause->literals[i];
        bool isLiteralNegated = (literal->exprType == Expression::Type::NEGATION);
        auto literalAtom = isLiteralNegated ?
            std::static_pointer_cast<NegationFormula>(literal)->child : literal;
        bool remove = false;
        if (literalAtom->exprType == Expression::Type::PREDICATE) {
            Substitution substitution;
            if (Unification::match(unitAtom, literalAtom, substitution)) {
                if (isUnitNegated == isLiteralNegated) return nullptr;
                else remove = true;
            }
        }
        if (remove) {
            if (!changed) {
                changed = true;
                newLiterals.reserve(clause->literals.size());
                for (size_t j = 0; j < i; ++j) newLiterals.push_back(clause->literals[j]);
            }
        }
        else if (changed) {
            newLiterals.push_back(literal);
        }
    }

    if (!changed) return clause;
    auto newClause = Clause::create("predicate_unit_simplification", clause, unitClause);
    newClause->literals = std::move(newLiterals);
    return newClause;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyDemodulation(
    const ClausePtr& clause, const ClausePtr& unitClause) const {
    if (!clause) return nullptr;
    assert(unitClause && unitClause->literals.size() == 1);

    if (!unitClause || unitClause->literals.size() != 1) return clause;
    auto unitLiteral = unitClause->literals.front();
    if (unitLiteral->exprType != Expression::Type::EQUALITY) return clause;
    auto equality = std::static_pointer_cast<EqualityFormula>(unitLiteral);
    TermPtr pattern, replacement;
    auto comparisonResult = lpo.compare(equality->left, equality->right);
    if (comparisonResult == Lpo::Result::GREATER) {
        pattern = equality->left;
        replacement = equality->right;
    }
    else if (comparisonResult == Lpo::Result::LESS) {
        pattern = equality->right;
        replacement = equality->left;
    }
    else return clause;

    std::function<ExpressionPtr(ExpressionPtr, bool)> rewrite =
        [&](ExpressionPtr expression, bool blockRoot) -> ExpressionPtr {
        if (!blockRoot && expression->isTerm()) {
            Substitution substitution;
            if (Unification::match(pattern, expression, substitution)) {
                return Unification::substitute(replacement, substitution);
            }
        }
        ExpressionPtr newExpression = nullptr;
        size_t count = expression->getChildCount();
        for (size_t i = 0; i < count; ++i) {
            auto child = expression->getChild(i);
            auto newChild = rewrite(child, false);
            if (newChild != child) {
                if (!newExpression) {
                    newExpression = expression->cloneShallow();
                }
                newExpression->setChild(i, newChild);
            }
        }
        return newExpression ? newExpression : expression;
    };

    auto selectionMask = selectLiterals(clause->literals);
    auto eligibleMask = areEligibleForParamodulation(clause->literals, selectionMask);

    std::vector<FormulaPtr> newLiterals;
    for (size_t i = 0; i < clause->literals.size(); ++i) {
        const auto& literal = clause->literals[i];
        ExpressionPtr newLiteralExpr = nullptr;
        if (eligibleMask[i] && literal->exprType == Expression::Type::EQUALITY) {
            auto equality = std::static_pointer_cast<EqualityFormula>(literal);
            auto cmp = lpo.compare(equality->left, equality->right);
            bool blockLeft = (cmp != Lpo::Result::LESS);
            bool blockRight = (cmp != Lpo::Result::GREATER);
            auto newLeft = rewrite(equality->left, blockLeft);
            auto newRight = rewrite(equality->right, blockRight);
            if (newLeft != equality->left || newRight != equality->right) {
                newLiteralExpr = std::make_shared<EqualityFormula>(
                    std::static_pointer_cast<Term>(newLeft),
                    std::static_pointer_cast<Term>(newRight));
            }
            else newLiteralExpr = literal;
        }
        else if (eligibleMask[i] && literal->exprType == Expression::Type::PREDICATE) {
            newLiteralExpr = rewrite(literal, true);
        }
        else newLiteralExpr = rewrite(literal, false);
        auto newLiteral = std::static_pointer_cast<Formula>(newLiteralExpr);
        if (newLiteral != literal) {
            if (newLiterals.empty()) newLiterals = clause->literals;
            newLiterals[i] = newLiteral;
        }
    }

    if (newLiterals.empty()) return clause;
    auto newClause = Clause::create("demodulation", clause, unitClause);
    newClause->literals = std::move(newLiterals);
    return newClause;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyClauseSubsumption(
    const ClausePtr& subsumed, const ClausePtr& subsuming) const {
    if (!subsumed) return nullptr;
    assert(subsuming);
    if (subsumed->literals.size() < subsuming->literals.size()) {
        return subsumed;
    }

    std::function<bool(size_t, const Substitution&)> checkRecursively =
        [&](size_t subsumingClauseIndex, const Substitution& currentSubstitution) -> bool {
        if (subsumingClauseIndex == subsuming->literals.size()) return true;
        auto patternLiteral = subsuming->literals[subsumingClauseIndex];
        for (const auto& targetLiteral : subsumed->literals) {
            Substitution nextSubstitution = currentSubstitution;
            if (Unification::match(patternLiteral, targetLiteral, nextSubstitution)) {
                if (checkRecursively(subsumingClauseIndex + 1, nextSubstitution)) {
                    return true;
                }
            }
        }
        return false;
    };

    if (checkRecursively(0, Substitution{})) return nullptr;
    return subsumed;
}

static bool isPairInstance(const ExpressionPtr& instanceL, const ExpressionPtr& instanceR,
                           const ExpressionPtr& patternL, const ExpressionPtr& patternR) {
    Substitution substitution;
    if (Unification::match(patternL, instanceL, substitution) &&
        Unification::match(patternR, instanceR, substitution)) return true;
    substitution.clear();
    if (Unification::match(patternL, instanceR, substitution) &&
        Unification::match(patternR, instanceL, substitution)) return true;
    return false;
}

static bool checkEqImpliedEquality(const ExpressionPtr& expr1, const ExpressionPtr& expr2,
                                   const TermPtr& eq1, const TermPtr& eq2) {
    assert(expr1 && expr2 && eq1 && eq2);
    assert(expr1->isTerm() && expr2->isTerm());
    if (expr1 == expr2) return true;

    if (expr1->exprType == expr2->exprType &&
        expr1->getChildCount() == expr2->getChildCount()) {
        bool expressionMatch = true;
        if (expr1->exprType == Expression::Type::FUNCTION) {
            auto function1 = std::static_pointer_cast<FunctionTerm>(expr1);
            auto function2 = std::static_pointer_cast<FunctionTerm>(expr2);
            if (function1->symbol != function2->symbol ||
                function1->distinct != function2->distinct) {
                expressionMatch = false;
            }
        }
        else if (expr1->exprType == Expression::Type::VARIABLE) {
            auto variable1 = std::static_pointer_cast<VariableTerm>(expr1);
            auto variable2 = std::static_pointer_cast<VariableTerm>(expr2);
            if (variable1->symbol != variable2->symbol) expressionMatch = false;
        }

        if (expressionMatch) {
            bool childrenMatch = true;
            size_t childCount = expr1->getChildCount();
            for (size_t i = 0; i < childCount; ++i) {
                if (!checkEqImpliedEquality(expr1->getChild(i), expr2->getChild(i), eq1, eq2)) {
                    childrenMatch = false;
                    break;
                }
            }
            if (childrenMatch) return true;
        }
    }

    return isPairInstance(expr1, expr2, eq1, eq2);
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyEqualitySubsumption(
    const ClausePtr& subsumed, const ClausePtr& unitClause) const {
    if (!subsumed) return nullptr;
    assert(unitClause);

    if (unitClause->literals.size() != 1) return subsumed;
    auto unitLiteral = unitClause->literals.front();
    if (unitLiteral->exprType != Expression::Type::EQUALITY) return subsumed;
    auto unitEquality = std::static_pointer_cast<EqualityFormula>(unitLiteral);

    for (const auto& literal : subsumed->literals) {
        if (literal->exprType == Expression::Type::EQUALITY) {
            auto equality = std::static_pointer_cast<EqualityFormula>(literal);
            if (checkEqImpliedEquality(equality->left, equality->right,
                                       unitEquality->left, unitEquality->right)) {
                return nullptr;
            }
        }
    }
    return subsumed;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyPositiveSimplifyReflect(
    const ClausePtr& clause, const ClausePtr& unitClause) const {
    if (!clause) return nullptr;
    assert(unitClause);

    if (unitClause->literals.size() != 1) return clause;
    auto unitLiteral = unitClause->literals.front();
    if (unitLiteral->exprType != Expression::Type::EQUALITY) return clause;
    auto unitEquality = std::static_pointer_cast<EqualityFormula>(unitLiteral);

    std::vector<FormulaPtr> newLiterals;
    bool changed = false;
    for (size_t i = 0; i < clause->literals.size(); ++i) {
        const auto& literal = clause->literals[i];
        bool remove = false;
        if (literal->exprType == Expression::Type::NEGATION) {
            auto negation = std::static_pointer_cast<NegationFormula>(literal);
            if (negation->child->exprType == Expression::Type::EQUALITY) {
                auto equality = std::static_pointer_cast<EqualityFormula>(negation->child);
                if (checkEqImpliedEquality(equality->left, equality->right,
                    unitEquality->left, unitEquality->right)) remove = true;
            }
        }
        if (remove) {
            if (!changed) {
                changed = true;
                newLiterals.reserve(clause->literals.size());
                for (size_t j = 0; j < i; ++j) {
                    newLiterals.push_back(clause->literals[j]);
                }
            }
        }
        else if (changed) newLiterals.push_back(literal);
    }

    if (!changed) return clause;
    auto newClause = Clause::create("positive_simplify-reflect", clause, unitClause);
    newClause->literals = std::move(newLiterals);
    return newClause;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyNegativeSimplifyReflect(
    const ClausePtr& clause, const ClausePtr& unitClause) const {
    if (!clause) return nullptr;
    assert(unitClause);

    if (unitClause->literals.size() != 1) return clause;
    auto unitLiteral = unitClause->literals.front();
    if (unitLiteral->exprType != Expression::Type::NEGATION) return clause;
    auto unitNegation = std::static_pointer_cast<NegationFormula>(unitLiteral);
    if (unitNegation->child->exprType != Expression::Type::EQUALITY) return clause;
    auto unitEquality = std::static_pointer_cast<EqualityFormula>(unitNegation->child);

    std::vector<FormulaPtr> newLiterals;
    bool changed = false;
    for (size_t i = 0; i < clause->literals.size(); ++i) {
        const auto& literal = clause->literals[i];
        bool remove = false;
        if (literal->exprType == Expression::Type::EQUALITY) {
            auto equality = std::static_pointer_cast<EqualityFormula>(literal);
            if (isPairInstance(equality->left, equality->right,
                unitEquality->left, unitEquality->right)) remove = true;
        }
        if (remove) {
            if (!changed) {
                changed = true;
                newLiterals.reserve(clause->literals.size());
                for (size_t j = 0; j < i; ++j) {
                    newLiterals.push_back(clause->literals[j]);
                }
            }
        }
        else if (changed) newLiterals.push_back(literal);
    }

    if (!changed) return clause;
    auto newClause = Clause::create("negative_simplify-reflect", clause, unitClause);
    newClause->literals = std::move(newLiterals);
    return newClause;
}

bool SuperpositionSolver::removeBoolLiterals(
    std::vector<FormulaPtr>& literals, bool* changed) const {
    auto it = literals.begin();
    while (it != literals.end()) {
        const auto& literal = *it;
        bool remove = false;
        if (literal->exprType == Expression::Type::BOOLEAN) {
            auto boolean = std::static_pointer_cast<BooleanFormula>(literal);
            if (boolean->value) return true; // tautology
            else remove = true;
        }
        else if (literal->exprType == Expression::Type::NEGATION) {
            auto negation = std::static_pointer_cast<NegationFormula>(literal);
            assert(negation->child);
            if (negation->child->exprType == Expression::Type::BOOLEAN) {
                auto boolean = std::static_pointer_cast<BooleanFormula>(negation->child);
                if (boolean->value) remove = true;
                else return true; // tautology
            }
        }
        if (remove) {
            it = literals.erase(it);
            if (changed) *changed = true;
        }
        else ++it;
    }
    return false; // not tautology
}

bool SuperpositionSolver::handleDistinctObjects(
    std::vector<FormulaPtr>& literals, bool* changed) const {
    auto getDistinctSymbol = [](const TermPtr& term) -> std::string {
        if (term->exprType != Expression::Type::FUNCTION) return "";
        auto functionTerm = std::static_pointer_cast<FunctionTerm>(term);
        if (functionTerm->distinct) {
            return functionTerm->symbol;
        }
        return "";
    };

    auto it = literals.begin();
    while (it != literals.end()) {
        FormulaPtr literal = *it;

        FormulaPtr atom;
        bool isNegated;
        if (literal->exprType == Expression::Type::NEGATION) {
            atom = std::static_pointer_cast<NegationFormula>(literal)->child;
            isNegated = true;
        }
        else {
            atom = literal;
            isNegated = false;
        }

        if (atom->exprType == Expression::Type::EQUALITY) {
            auto equality = std::static_pointer_cast<EqualityFormula>(atom);
            std::string leftSymbol = getDistinctSymbol(equality->left);
            std::string rightSymbol = getDistinctSymbol(equality->right);

            if (!leftSymbol.empty() && !rightSymbol.empty()) {
                bool symbolsIdentical = (leftSymbol == rightSymbol);
                if (symbolsIdentical != isNegated) {
                    return true;
                }
                it = literals.erase(it);
                if (changed) *changed = true;
                continue;
            }
        }
        ++it;
    }
    return false;
}

void SuperpositionSolver::standardizeVariables(ClausePtr& clause) {
    if (clause->literals.empty()) return;
    auto junction = std::make_shared<JunctionFormula>(
        JunctionFormula::Operator::OR, clause->literals);
    auto standardized = transformer.standardizeVariables(junction, true);
    assert(standardized->exprType == Expression::Type::JUNCTION);
    auto standardizedJunction = std::static_pointer_cast<JunctionFormula>(standardized);
    clause->literals = standardizedJunction->operands;
}

static float getExpressionWeight(const ExpressionPtr& expr,
                                 float predicateWeight = 2.0f,
                                 float functionWeight = 2.0f,
                                 float variableWeight = 1.0f) {
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

static std::vector<bool> selectNothing(const std::vector<FormulaPtr>& literals) {
    return std::vector<bool>(literals.size(), false);
}

static std::vector<bool> selectDiffNegLiteral(const std::vector<FormulaPtr>& literals) {
    constexpr float PREDICATE_WEIGHT = 2.0f;
    constexpr float TIE_BREAKER_COEF = 0.01f;

    std::vector<bool> selection(literals.size(), false);
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

static std::vector<bool> selectComplex(const std::vector<FormulaPtr>& literals) {
    for (size_t i = 0; i < literals.size(); ++i) {
        if (literals[i]->exprType == Expression::Type::NEGATION) {
            auto negation = std::static_pointer_cast<NegationFormula>(literals[i]);
            if (negation->child->exprType == Expression::Type::EQUALITY) {
                auto equality = std::static_pointer_cast<EqualityFormula>(negation->child);
                if (equality->left->exprType == Expression::Type::VARIABLE &&
                    equality->right->exprType == Expression::Type::VARIABLE) {
                    std::vector<bool> selection(literals.size(), false);
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
        std::vector<bool> selection(literals.size(), false);
        selection[bestIndex] = true;
        return selection;
    }

    return selectDiffNegLiteral(literals);
}

static std::vector<bool> SelectComplexExceptRRHorn(const std::vector<FormulaPtr>& literals) {
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

std::vector<bool> SuperpositionSolver::selectLiterals(
    const std::vector<FormulaPtr>& literals) const {
    // The mask must contain at least one negative literal if non-empty
    return SelectComplexExceptRRHorn(literals);
}

std::vector<bool> SuperpositionSolver::areEligibleForResolution(
    const std::vector<FormulaPtr>& literals,
    const std::vector<bool>& selectionMask) const {

    auto computeMaximalLiterals = [this](
        const std::vector<FormulaPtr>& literals,
        const std::vector<bool>& scopeMask) -> std::vector<bool> {
        std::vector<bool> resultMask = scopeMask;
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
        return computeMaximalLiterals(literals, std::vector<bool>(literals.size(), true));
    }

    std::vector<bool> negLiteralsMask(literals.size(), false);
    std::vector<bool> posLiteralsMask(literals.size(), false);
    for (size_t i = 0; i < literals.size(); ++i) {
        if (selectionMask[i]) {
            if (literals[i]->exprType == Expression::Type::NEGATION) {
                negLiteralsMask[i] = true;
            }
            else posLiteralsMask[i] = true;
        }
    }
    std::vector<bool> negMaxLiteralsMask = computeMaximalLiterals(literals, negLiteralsMask);
    std::vector<bool> posMaxLiteralsMask = computeMaximalLiterals(literals, posLiteralsMask);
    for (size_t i = 0; i < literals.size(); ++i) {
        negMaxLiteralsMask[i] = negMaxLiteralsMask[i] || posMaxLiteralsMask[i];
    }
    return negMaxLiteralsMask;
}

std::vector<bool> SuperpositionSolver::areEligibleForParamodulation(
    const std::vector<FormulaPtr>& literals,
    const std::vector<bool>& selectionMask,
    bool strictlyMaximal) const {

    assert(literals.size() == selectionMask.size());
    std::vector<bool> result(literals.size(), false);
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

ProofNodePtr SuperpositionSolver::reconstructProof(const ClausePtr& clause,
    std::map<ClausePtr, ProofNodePtr>& cache) const {
    assert(clause);
    if (!clause) return nullptr;
    if (cache.count(clause)) return cache[clause];
    if (clause->input) {
        return clause->input;
    }

    FormulaPtr formula;
    if (clause->literals.empty()) {
        formula = std::make_shared<BooleanFormula>(false);
    }
    else if (clause->literals.size() == 1) {
        formula = clause->literals.front();
    }
    else {
        formula = std::make_shared<JunctionFormula>(
            JunctionFormula::Operator::OR, clause->literals);
    }
    std::vector<ProofNodePtr> parents;
    if (clause->parent1) parents.push_back(reconstructProof(clause->parent1, cache));
    if (clause->parent2) parents.push_back(reconstructProof(clause->parent2, cache));

    assert(formula && !parents.empty() && !clause->rule.empty());
    auto node = ProofStep::create(std::move(formula), ProofNode::Type::INFERENCE,
        clause->rule, std::move(parents));
    cache[clause] = node;
    return node;
}

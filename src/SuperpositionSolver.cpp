#include "SuperpositionSolver.hpp"

#include "ExpressionUtils.hpp"
#include "Unification.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
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

    void markRedundant() {
        if (redundant) return;
        redundant = true;
        auto it = children.begin();
        while (it != children.end()) {
            if (auto childPtr = it->lock()) {
                childPtr->markRedundant();
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
    }

    bool removeClause(const ClausePtr& clause) {
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

    std::vector<ClauseSelector::SelectionStrategy> selectionStrategies = {
        { 4, [](const ClausePtr& clause, uint64_t) { return (float)clause->literals.size(); } },
        { 1, [](const ClausePtr&, uint64_t id) { return (float)id; } }
    };

    ClauseSelector unprocessedClauses(selectionStrategies);
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
        if (procClause->isRedundant()) continue;
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

std::vector<bool> SuperpositionSolver::selectLiterals(
    const std::vector<FormulaPtr>& literals) const {
    // The mask must contain at least one negative literal if non-empty
    return std::vector<bool>(literals.size(), false);
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
                    if (scopeMask[j] && i != j) {
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

#pragma once

#include "Expression.hpp"
#include "FolSatSolver.hpp"
#include "Lpo.hpp"

#include <functional>
#include <queue>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace SuperpositionSolverUtils {

using Literals = std::vector<FormulaPtr>;
using Mask = std::vector<bool>;

class LiteralSelector {
public:
    using SelectorFunc = std::function<Mask(const Literals&)>;

    explicit LiteralSelector(SelectorFunc selectorFunc, const Lpo& lpo);

    Mask selectLiterals(const Literals& literals) const;
    Mask areEligibleForResolution(const Literals& literals, const Mask& selectionMask) const;
    Mask areEligibleForParamodulation(const Literals& literals, const Mask& selectionMask,
        bool strictlyMaximal = false) const;

    static Mask selectNothing(const Literals& literals);
    static Mask selectDiffNegLiteral(const Literals& literals);
    static Mask selectComplex(const Literals& literals);
    static Mask selectComplexExceptRRHorn(const Literals& literals);

private:
    SelectorFunc selectorFunc;
    const Lpo& lpo;
};

struct Clause;
using ClausePtr = std::shared_ptr<Clause>;
using Clauses = std::vector<ClausePtr>;

struct Clause {
    const Literals literals;

    const ProofNodePtr input;

    const std::string rule;
    const ClausePtr parent1;
    const ClausePtr parent2;

private:
    Clause(Literals literals, ProofNodePtr input);
    Clause(Literals literals, std::string rule,
        ClausePtr parent1, ClausePtr parent2 = nullptr);

    std::vector<std::weak_ptr<Clause>> children;

    Mask selectedLiteralsMask;
    Mask eligibleForResolutionMask;
    Mask eligibleForParamodulationMask;
    Mask eligibleForParamodulationSMMask;

public:
    static ClausePtr create(Literals literals, ProofNodePtr input);
    static ClausePtr create(Literals literals, std::string rule,
        ClausePtr parent1, ClausePtr parent2 = nullptr);

    Mask getSelectedLiteralsMask(const LiteralSelector& selector);
    Mask getEligibleForResolutionMask(const LiteralSelector& selector);
    Mask getEligibleForParamodulationMask(const LiteralSelector& selector,
        bool strictlyMaximal = false);

    template<typename Func>
    void forEachChild(Func callback) {
        size_t i = 0;
        while (i < children.size()) {
            if (auto child = children[i].lock()) {
                callback(child);
                ++i;
            }
            else {
                children[i] = children.back();
                children.pop_back();
            }
        }
    }
};

class ClauseSelector {
public:
    using WeightEvaluator = std::function<float(
        const ClausePtr& clause, uint64_t id, const Lpo& lpo)>;

    struct SelectionStrategy {
        size_t quota;
        WeightEvaluator evaluator;
    };

    explicit ClauseSelector(const std::vector<SelectionStrategy>& strategies, const Lpo& lpo);

    bool isEmpty() const;
    void addClause(const ClausePtr& clause);
    ClausePtr selectClause();
    bool removeClause(const ClausePtr& clause);

    static float fifoWeightEvaluator(const ClausePtr&, uint64_t id, const Lpo&);
    static float clauseWeightEvaluator(const ClausePtr& clause, uint64_t, const Lpo&);
    static float refinedWeightEvaluator(const ClausePtr& clause, uint64_t, const Lpo& lpo);

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

    const Lpo& lpo;

    uint64_t clauseIdCounter = 0;
    size_t currentQueueIndex = 0;
    size_t quotaUsed = 0;
};

class ClauseIndex {
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

float getExpressionWeight(const ExpressionPtr& expr,
                          float predicateWeight = 2.0f,
                          float functionWeight = 2.0f,
                          float variableWeight = 1.0f);

} // namespace SuperpositionSolverUtils

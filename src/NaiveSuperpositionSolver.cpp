#include "NaiveSuperpositionSolver.hpp"

#include "ClauseUtils.hpp"
#include "ExpressionBuilder.hpp"
#include "ExpressionUtils.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>

using namespace ClauseUtils;

struct NaiveSuperpositionSolver::Clause {
    const ProofNodePtr input;

    const std::string rule;
    const ClausePtr parent1;
    const ClausePtr parent2;

    std::vector<FormulaPtr> literals;

    Clause(ProofNodePtr input) :
        input(input), parent1(nullptr), parent2(nullptr) {}
    Clause(std::string rule, ClausePtr parent1, ClausePtr parent2 = nullptr) :
        rule(std::move(rule)), input(nullptr), parent1(parent1), parent2(parent2) {}
};

void NaiveSuperpositionSolver::setTimeLimit(int seconds) {
    timeLimitSeconds = static_cast<double>(seconds);
}

void NaiveSuperpositionSolver::setMemoryLimit(int megabytes) {
    memoryLimitMegabytes = megabytes;
}

FolSatSolver::Result NaiveSuperpositionSolver::solve(const std::vector<ProofNodePtr>& clauses) {
    auto startTime = std::chrono::steady_clock::now();
    size_t iterationCounter = 0;

    std::deque<ClausePtr> unprocessedClauses;
    std::vector<ClausePtr> processedClauses;
    transformer = ExpressionTransformer();
    proofRoot = nullptr;

    for(size_t i = 0; i < clauses.size(); ++i) {
        const auto& clause = clauses[i]->getFormula();
        assert(clause);
        assert(clause->exprType == Expression::Type::JUNCTION);
        auto junction = std::static_pointer_cast<JunctionFormula>(clause->clone());
        ClausePtr inputClause = std::make_shared<Clause>(clauses[i]);
        inputClause->literals = junction->operands;

        std::vector<FormulaPtr> workingLiterals = junction->operands;
        bool changed = false;
        bool isTautology = removeBoolLiterals(workingLiterals, &changed);
        if (!isTautology) isTautology = handleDistinctObjects(workingLiterals, &changed);
        if (!isTautology) {
            ClausePtr finalClause = inputClause;
            if (changed) {
                finalClause = std::make_shared<Clause>("simplification", inputClause);
                finalClause->literals = std::move(workingLiterals);
            }
            if (finalClause->literals.empty()) {
                proofRoot = finalClause;
                return FolSatSolver::Result::UNSATISFIABLE;
            }
            standardizeVariables(finalClause);
            unprocessedClauses.push_back(finalClause);
        }
    }

    while (!unprocessedClauses.empty()) {
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

        ClausePtr unprocClause = unprocessedClauses.front();
        unprocessedClauses.pop_front();

        std::vector<ClausePtr> inferredClauses;
        applyFactoring(unprocClause, inferredClauses);
        applyEqualityResolution(unprocClause, inferredClauses);
        applyEqualityFactoring(unprocClause, inferredClauses);
        for (const auto& procClause : processedClauses) {
            applyBinaryResolution(procClause, unprocClause, inferredClauses);
            applySuperposition(procClause, unprocClause, inferredClauses);
        }

        for (auto& inferredClause : inferredClauses) {
            // temp. begin
            bool isTautology = false;
            for (const auto& literal : inferredClause->literals) {
                if (literal->exprType == Expression::Type::EQUALITY) {
                    auto equality = std::static_pointer_cast<EqualityFormula>(literal);
                    if (ExpressionUtils::areAlphaEquivalent(equality->left, equality->right)) {
                        isTautology = true;
                        break;
                    }
                }
            }
            // temp. end
            std::vector<FormulaPtr> workingLiterals = inferredClause->literals;
            bool changed = false;
            if(!isTautology) isTautology = handleDistinctObjects(workingLiterals, &changed);
            if (!isTautology) {
                ClausePtr finalClause = inferredClause;
                if (changed) {
                    finalClause = std::make_shared<Clause>("simplification", inferredClause);
                    finalClause->literals = std::move(workingLiterals);
                }
                if (finalClause->literals.empty()) {
                    proofRoot = finalClause;
                    return FolSatSolver::Result::UNSATISFIABLE;
                }
                standardizeVariables(finalClause);
                unprocessedClauses.push_back(finalClause);
            }
        }
        processedClauses.push_back(unprocClause);
    }
    return FolSatSolver::Result::SATISFIABLE;
}

ProofNodePtr NaiveSuperpositionSolver::getProof() const {
    if (!proofRoot) return nullptr;
    std::map<ClausePtr, ProofNodePtr> cache;
    return reconstructProof(proofRoot, cache);
}

void NaiveSuperpositionSolver::applyBinaryResolution(
    const ClausePtr& clause1, const ClausePtr& clause2,
    std::vector<ClausePtr>& resolvents) const {
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
                    auto newClause = std::make_shared<Clause>(rule, lClause, rClause);
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

void NaiveSuperpositionSolver::applyFactoring(
    const ClausePtr& clause, std::vector<ClausePtr>& factors) const {
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
                auto newClause = std::make_shared<Clause>(rule, clause);
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

void NaiveSuperpositionSolver::applySuperposition(
    const ClausePtr& clause1, const ClausePtr& clause2,
    std::vector<ClausePtr>& inferredClauses) const {
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
                auto newClause = std::make_shared<Clause>(rule, fromClause, intoClause);
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
                inferredClauses.push_back(newClause);
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

void NaiveSuperpositionSolver::applyEqualityResolution(const ClausePtr& clause,
    std::vector<ClausePtr>& inferredClauses) const {
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
                auto newClause = std::make_shared<Clause>(rule, clause);
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

void NaiveSuperpositionSolver::applyEqualityFactoring(const ClausePtr& clause,
    std::vector<ClausePtr>& inferredClauses) const {
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
                        auto newClause = std::make_shared<Clause>(rule, clause);
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

bool NaiveSuperpositionSolver::removeBoolLiterals(
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

bool NaiveSuperpositionSolver::handleDistinctObjects(
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

void NaiveSuperpositionSolver::standardizeVariables(ClausePtr& clause) {
    if (clause->literals.empty()) return;
    auto junction = std::make_shared<JunctionFormula>(
        JunctionFormula::Operator::OR, clause->literals);
    auto standardized = transformer.standardizeVariables(junction, true);
    assert(standardized->exprType == Expression::Type::JUNCTION);
    auto standardizedJunction = std::static_pointer_cast<JunctionFormula>(standardized);
    clause->literals = standardizedJunction->operands;
}

std::vector<bool> NaiveSuperpositionSolver::selectLiterals(
    const std::vector<FormulaPtr>& literals) const {
    // The mask must contain at least one negative literal if non-empty
    return std::vector<bool>(literals.size(), false);
}

std::vector<bool> NaiveSuperpositionSolver::areEligibleForResolution(
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

std::vector<bool> NaiveSuperpositionSolver::areEligibleForParamodulation(
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

ProofNodePtr NaiveSuperpositionSolver::reconstructProof(const ClausePtr& clause,
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

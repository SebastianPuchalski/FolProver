#include "SuperpositionSolver.hpp"

#include "ExpressionUtils.hpp"
#include "Unification.hpp"
#include "Utils.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

using namespace SuperpositionSolverUtils;
using namespace Unification;

SuperpositionSolver::SuperpositionSolver() :
    literalSelector(LiteralSelector::selectComplexExceptRRHorn, lpo) {
}

void SuperpositionSolver::setTimeLimit(int seconds) {
    timeLimitSeconds = static_cast<double>(seconds);
}

void SuperpositionSolver::setMemoryLimit(int megabytes) {
    memoryLimitMegabytes = megabytes;
}

FolSatSolver::Result SuperpositionSolver::solve(const std::vector<ProofNodePtr>& clauses) {
    auto resourceLimitState = initResourceLimitState();

    ClauseSelector unprocessedClauses = createClauseSelector();
    ClauseIndex processedClauses;
    transformer = ExpressionTransformer();
    proofRoot = nullptr;

    auto contradiction = loadInitialClauses(clauses, unprocessedClauses);
    if (contradiction) return Result::UNSATISFIABLE;

    while (!unprocessedClauses.isEmpty()) {
        auto checkResult = checkResourceLimits(resourceLimitState);
        if (checkResult != Result::UNKNOWN) return checkResult;

        ClausePtr givenClause = unprocessedClauses.selectClause();
        if (!givenClause) continue;

        givenClause = simplifyForward(givenClause, processedClauses);
        if (!givenClause) continue;
        if (givenClause->literals.empty()) {
            proofRoot = givenClause;
            return Result::UNSATISFIABLE;
        }

        Clauses derivedClauses;
        simplifyBackward(processedClauses, givenClause, derivedClauses, unprocessedClauses);
        generateInferences(givenClause, processedClauses, derivedClauses);
        processedClauses.addClause(givenClause);

        for (auto clause : derivedClauses) {
            standardizeVariables(clause);
            clause = simplifyCheapForward(clause, processedClauses);
            clause = applyDistinctObjectSimplification(clause);
            if (!clause) continue;
            if (clause->literals.empty()) {
                proofRoot = clause;
                return Result::UNSATISFIABLE;
            }
            unprocessedClauses.addClause(clause);
        }
    }
    return Result::SATISFIABLE;
}

ProofNodePtr SuperpositionSolver::getProof() const {
    if (!proofRoot) return nullptr;
    std::map<ClausePtr, ProofNodePtr> cache;
    return reconstructProof(proofRoot, cache);
}

std::pair<double, size_t> SuperpositionSolver::initResourceLimitState() const {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> currentTime = now.time_since_epoch();
    return { currentTime.count(), 0 };
}

FolSatSolver::Result SuperpositionSolver::checkResourceLimits(
    std::pair<double, size_t>& state) const {
    if (++state.second >= 128) {
        state.second = 0;
        if (timeLimitSeconds > 0.0) {
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> currentTime = now.time_since_epoch();
            if ((currentTime.count() - state.first) > timeLimitSeconds) {
                return Result::TIME_OUT;
            }
        }
        if (memoryLimitMegabytes > 0) {
            auto memoryLimitBytes = memoryLimitMegabytes * 1024 * 1024;
            if (getPeakMemoryUsageInBytes() > memoryLimitBytes) {
                return Result::MEMORY_OUT;
            }
        }
    }
    return Result::UNKNOWN;
}

SuperpositionSolver::ClauseSelector SuperpositionSolver::createClauseSelector() const {
    std::vector<ClauseSelector::SelectionStrategy> selectionStrategies = {
        { 3, ClauseSelector::refinedWeightEvaluator },
        { 1, ClauseSelector::clauseWeightEvaluator },
        { 1, ClauseSelector::fifoWeightEvaluator }
    };
    return ClauseSelector(selectionStrategies, lpo);
}

bool SuperpositionSolver::loadInitialClauses(const std::vector<ProofNodePtr>& clauses,
    ClauseSelector& unprocessedClauses) {
    for (size_t i = 0; i < clauses.size(); ++i) {
        const auto& formula = clauses[i]->getFormula();
        assert(formula);
        assert(formula->exprType == Expression::Type::JUNCTION);
        auto junction = std::static_pointer_cast<JunctionFormula>(formula->clone());
        assert(junction->op == JunctionFormula::Operator::OR);
        ClausePtr inputClause = Clause::create(std::move(junction->operands), clauses[i]);

        inputClause = applyBooleanSimplification(inputClause);
        inputClause = applyDistinctObjectSimplification(inputClause);
        if (!inputClause) continue;

        if (inputClause->literals.empty()) {
            proofRoot = inputClause;
            return true;
        }

        standardizeVariables(inputClause);
        unprocessedClauses.addClause(inputClause);
    }
    return false;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::simplifyForward(
    const ClausePtr& clauseToSimplify, const ClauseIndex& index) const {
    // (RN), (RP), (NS), (PS), (CLC), (DR), (DD), (DE), (CS), (ES), (TD)
    if (!clauseToSimplify) return nullptr;

    auto current = clauseToSimplify;
    bool changed;

    auto update = [&](ClausePtr result) {
        if (result != current) {
            current = result;
            changed = true;
        }
    };

    do {
        changed = false;

        update(applyTautologyDeletion(current));
        if (!current) return nullptr;

        update(applyDestructiveEqualityResolution(current));
        if (!current) return nullptr;

        update(applyDeletionOfResolvedLiterals(current));
        if (!current) return nullptr;

        update(applyDeletionOfDuplicateLiterals(current));
        if (!current) return nullptr;

        for (const auto& unitClause : index.getUnitClauses()) {
            update(applyPredicateUnitSimplification(current, unitClause));
            if (!current) return nullptr;
        }

        for (const auto& unitClause : index.getUnitClauses()) {
            update(applyDemodulation(current, unitClause));
            if (!current) return nullptr;
        }

        for (const auto& unitClause : index.getUnitClauses()) {
            update(applyPositiveSimplifyReflect(current, unitClause));
            if (!current) return nullptr;
        }

        for (const auto& unitClause : index.getUnitClauses()) {
            update(applyNegativeSimplifyReflect(current, unitClause));
            if (!current) return nullptr;
        }

        for (const auto& procClause : index.getClauses()) {
            update(applyClauseSubsumption(current, procClause));
            if (!current) return nullptr;
        }

        for (const auto& unitClause : index.getUnitClauses()) {
            update(applyEqualitySubsumption(current, unitClause));
            if (!current) return nullptr;
        }

    } while (changed);

    return current;
}

SuperpositionSolver::ClausePtr SuperpositionSolver::simplifyCheapForward(
    const ClausePtr& clauseToSimplify, const ClauseIndex& index) const {
    // efficiently implemented subset of (RN), (RP), (NS), (PS), (CLC), (DR), (DD), (DE), (TD)
    if (!clauseToSimplify) return nullptr;
    return applyTautologyDeletion(clauseToSimplify);
}

void SuperpositionSolver::simplifyBackward(ClauseIndex& indexToSimplify, const ClausePtr& clause,
    Clauses& reducedClauses, ClauseSelector& unprocessedClauses) const {
    assert(clause);

    auto deleteAndReplace = [&](const ClausePtr& oldClause,
                                const ClausePtr& newClause = nullptr) {
        assert(oldClause);
        if (oldClause == newClause) return;
        oldClause->forEachChild([&](ClausePtr child) {
            if (child->origin != Clause::Origin::SIMPLIFICATION) {
                unprocessedClauses.removeClause(child);
            }
            /*else {
                if (child != newClause) {
                    if (unprocessedClauses.removeClause(child)) {
                        reducedClauses.push_back(child->parent1);
                    }
                }
            }*/
        });
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

void SuperpositionSolver::generateInferences(
    const ClausePtr& clause, const ClauseIndex& index, Clauses& inferredClauses) const {
    applyFactoring(clause, inferredClauses);
    applyEqualityResolution(clause, inferredClauses);
    applyEqualityFactoring(clause, inferredClauses);
    for (const auto& procClause : index.getClauses()) {
        applyBinaryResolution(procClause, clause, inferredClauses);
        applySuperposition(procClause, clause, inferredClauses);
    }
    applySuperposition(clause, clause, inferredClauses);
}

void SuperpositionSolver::standardizeVariables(ClausePtr& clause) {
    // Violates immutability contract. Safe because variable renaming
    // (alpha-conversion) does not change the clause's logical identity.
    if (clause->literals.empty()) return;
    auto junction = std::make_shared<JunctionFormula>(
        JunctionFormula::Operator::OR, clause->literals);
    auto standardized = transformer.standardizeVariables(junction, true);
    assert(standardized->exprType == Expression::Type::JUNCTION);
    auto standardizedJunction = std::static_pointer_cast<JunctionFormula>(standardized);
    assert(standardizedJunction->operands.size() == clause->literals.size());
    for (size_t i = 0; i < clause->literals.size(); ++i) {
        assert(standardizedJunction->operands[i] == clause->literals[i]);
    }
}

ClausePtr SuperpositionSolver::applyBooleanSimplification(const ClausePtr& clause) const {
    if (!clause) return nullptr;

    Literals newLiterals;
    newLiterals.reserve(clause->literals.size());

    for (const auto& literal : clause->literals) {
        bool removeLiteral = false;
        if (literal->exprType == Expression::Type::BOOLEAN) {
            auto boolean = std::static_pointer_cast<BooleanFormula>(literal);
            if (boolean->value) return nullptr;
            else removeLiteral = true;
        }
        else if (literal->exprType == Expression::Type::NEGATION) {
            auto negation = std::static_pointer_cast<NegationFormula>(literal);
            assert(negation->child);
            if (negation->child->exprType == Expression::Type::BOOLEAN) {
                auto boolean = std::static_pointer_cast<BooleanFormula>(negation->child);
                if (boolean->value) removeLiteral = true;
                else return nullptr;
            }
        }
        if (!removeLiteral) newLiterals.push_back(literal);
    }

    if (newLiterals.size() == clause->literals.size()) return clause;
    return Clause::create(std::move(newLiterals), "boolean_simplification", true, clause);
}

ClausePtr SuperpositionSolver::applyDistinctObjectSimplification(const ClausePtr& clause) const {
    if (!clause) return nullptr;

    Literals newLiterals;
    newLiterals.reserve(clause->literals.size());

    auto getDistinctSymbol = [](const TermPtr& term) -> std::string {
        if (term->exprType != Expression::Type::FUNCTION) return "";
        auto functionTerm = std::static_pointer_cast<FunctionTerm>(term);
        if (functionTerm->distinct) {
            return functionTerm->symbol;
        }
        return "";
    };

    for (const auto& literal : clause->literals) {
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
                    return nullptr;
                }
                continue;
            }
        }
        newLiterals.push_back(literal);
    }

    if (newLiterals.size() == clause->literals.size()) return clause;
    return Clause::create(std::move(newLiterals), "distinct_object_simplification", true, clause);
}

void SuperpositionSolver::applyBinaryResolution(
    const ClausePtr& clause1, const ClausePtr& clause2,
    Clauses& resolvents) const {
    // Skip self-resolution: it generates valid macro-steps, but they are redundant for completeness
    if (clause1 == clause2) return;

    auto resolve = [&](const ClausePtr& lClause, const ClausePtr& rClause) {
        auto lEligibleMask = lClause->getEligibleForParamodulationMask(literalSelector, false);
        auto rEligibleMask = rClause->getEligibleForResolutionMask(literalSelector);

        for (size_t i = 0; i < lClause->literals.size(); ++i) {
            if (!lEligibleMask[i]) continue;
            auto lLiteral = lClause->literals[i];
            if (lLiteral->exprType != Expression::Type::PREDICATE) continue;

            for (size_t j = 0; j < rClause->literals.size(); ++j) {
                if (!rEligibleMask[j]) continue;
                auto rLiteral = rClause->literals[j];
                if (rLiteral->exprType != Expression::Type::NEGATION) continue;
                auto negation = std::static_pointer_cast<NegationFormula>(rLiteral);

                Substitution mgu;
                if (unify(lLiteral, negation->child, mgu)) {
                    Literals newLiterals;
                    for (size_t k = 0; k < lClause->literals.size(); ++k) {
                        if (k != i) {
                            auto newLiteral = substitute(lClause->literals[k], mgu);
                            newLiterals.push_back(std::static_pointer_cast<Formula>(newLiteral));
                        }
                    }
                    for (size_t k = 0; k < rClause->literals.size(); ++k) {
                        if (k != j) {
                            auto newLiteral = substitute(rClause->literals[k], mgu);
                            newLiterals.push_back(std::static_pointer_cast<Formula>(newLiteral));
                        }
                    }
                    auto rule = "resolution";
                    auto newClause = Clause::create(std::move(newLiterals), rule, false, lClause, rClause);
                    resolvents.push_back(newClause);
                }
            }
        }
    };

    resolve(clause1, clause2);
    resolve(clause2, clause1);
}

void SuperpositionSolver::applyFactoring(
    const ClausePtr& clause, Clauses& factors) const {
    size_t literalCount = clause->literals.size();
    if (literalCount < 2) return;

    auto eligibleMask = clause->getEligibleForParamodulationMask(literalSelector);

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
                Literals newLiterals;
                for (size_t k = 0; k < literalCount; ++k) {
                    if (k != i) {
                        auto newLiteral = substitute(clause->literals[k], mgu);
                        newLiterals.push_back(std::static_pointer_cast<Formula>(newLiteral));
                    }
                }
                auto rule = "factoring";
                auto newClause = Clause::create(std::move(newLiterals), rule, false, clause);
                factors.push_back(newClause);
            }
        }
    }
}

void SuperpositionSolver::applySuperposition(
    const ClausePtr& clause1, const ClausePtr& clause2,
    Clauses& paramodulants) const {

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
                Literals newLiterals;
                for (size_t i = 0; i < fromClause->literals.size(); ++i) {
                    if (i != fromLiteralIndex) {
                        auto literal = std::static_pointer_cast<Formula>(substitute(fromClause->literals[i], mgu));
                        newLiterals.push_back(literal);
                    }
                }
                for (size_t i = 0; i < intoClause->literals.size(); ++i) {
                    if (i != intoLiteralIndex) {
                        auto literal = std::static_pointer_cast<Formula>(substitute(intoClause->literals[i], mgu));
                        newLiterals.push_back(literal);
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
                        newLiterals.push_back(literal);
                    }
                }
                auto rule = "superposition";
                auto newClause = Clause::create(std::move(newLiterals), rule, false, fromClause, intoClause);
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
        const ClausePtr& fromClause, const ClausePtr& intoClause) {
        auto fromEligibleMask = fromClause->getEligibleForParamodulationMask(literalSelector, false);
        auto intoEligibleMask = intoClause->getEligibleForResolutionMask(literalSelector);

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

    if (clause1 != clause2) {
        processClausePair(clause1, clause2);
        processClausePair(clause2, clause1);
    }
    else {
        std::function<void(ExpressionPtr)> renameVariables = [&](const ExpressionPtr& expr) {
            if (expr->exprType == Expression::Type::VARIABLE) {
                auto variable = std::static_pointer_cast<VariableTerm>(expr);
                variable->symbol += "_c";
            }
            auto childCount = expr->getChildCount();
            for (size_t i = 0; i < childCount; ++i) {
                renameVariables(expr->getChild(i));
            }
        };

        Literals literalsRenamed;
        literalsRenamed.reserve(clause1->literals.size());
        for (const auto& literal : clause1->literals) {
            auto literalClone = std::static_pointer_cast<Formula>(literal->clone());
            renameVariables(literalClone);
            literalsRenamed.push_back(literalClone);
        }
        auto clauseCopy = Clause::create(std::move(literalsRenamed), "renaming", false, clause1);
        processClausePair(clause1, clauseCopy);
    }
}

void SuperpositionSolver::applyEqualityResolution(const ClausePtr& clause,
    Clauses& inferredClauses) const {
    auto eligibleMask = clause->getEligibleForResolutionMask(literalSelector);

    for (size_t i = 0; i < clause->literals.size(); ++i) {
        if (eligibleMask[i]) {
            auto literal = clause->literals[i];
            if (literal->exprType != Expression::Type::NEGATION) continue;
            auto negation = std::static_pointer_cast<NegationFormula>(literal);
            if (negation->child->exprType != Expression::Type::EQUALITY) continue;
            auto equality = std::static_pointer_cast<EqualityFormula>(negation->child);
            Substitution mgu;
            if (unify(equality->left, equality->right, mgu)) {
                Literals newLiterals;
                for (size_t j = 0; j < clause->literals.size(); ++j) {
                    if (j != i) {
                        newLiterals.push_back(std::static_pointer_cast<Formula>(
                            substitute(clause->literals[j], mgu)));
                    }
                }
                auto rule = "equality_resolution";
                auto newClause = Clause::create(std::move(newLiterals), rule, false, clause);
                inferredClauses.push_back(newClause);
            }
        }
    }
}

void SuperpositionSolver::applyEqualityFactoring(const ClausePtr& clause,
    Clauses& inferredClauses) const {
    size_t literalCount = clause->literals.size();
    if (literalCount < 2) return;

    auto eligibleMask = clause->getEligibleForParamodulationMask(literalSelector);

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
                        Literals newLiterals;
                        auto tSub = substitute(t, mgu);
                        auto vSub = substitute(v, mgu);
                        auto newEquality = std::make_shared<EqualityFormula>(
                            std::static_pointer_cast<Term>(tSub),
                            std::static_pointer_cast<Term>(vSub));
                        auto newInequality = std::make_shared<NegationFormula>(newEquality);
                        newLiterals.push_back(newInequality);
                        for (size_t k = 0; k < literalCount; ++k) {
                            if (k != i) {
                                auto newLiteral = substitute(clause->literals[k], mgu);
                                newLiterals.push_back(std::static_pointer_cast<Formula>(newLiteral));
                            }
                        }
                        auto rule = "equality_factoring";
                        auto newClause = Clause::create(std::move(newLiterals), rule, false, clause);
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

    Literals uniqueLiterals;
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
    return Clause::create(std::move(uniqueLiterals),
        "deletion_of_duplicate_literals", true, clause);
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyDeletionOfResolvedLiterals(
    const ClausePtr& clause) const {
    if (!clause) return nullptr;

    Literals literals;
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
    return Clause::create(std::move(literals),
        "deletion_of_resolved_literals", true, clause);
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
            Literals newLiterals;
            newLiterals.reserve(clause->literals.size() - 1);
            for (size_t j = 0; j < clause->literals.size(); ++j) {
                if (i == j) continue;
                auto newLiteral = Unification::substitute(clause->literals[j], mgu);
                newLiterals.push_back(std::static_pointer_cast<Formula>(newLiteral));
            }
            return Clause::create(std::move(newLiterals),
                "destructive_equality_resolution", true, clause);
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

    Literals newLiterals;
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
                for (size_t j = 0; j < i; ++j) {
                    newLiterals.push_back(clause->literals[j]);
                }
            }
        }
        else if (changed) {
            newLiterals.push_back(literal);
        }
    }

    if (!changed) return clause;
    for (auto& literal : newLiterals) {
        literal = std::static_pointer_cast<Formula>(literal->clone());
    }
    return Clause::create(std::move(newLiterals),
        "predicate_unit_simplification", true, clause, unitClause);
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

    auto eligibleMask = clause->getEligibleForParamodulationMask(literalSelector);

    Literals newLiterals;
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
    return Clause::create(std::move(newLiterals),
        "demodulation", true, clause, unitClause);
}

SuperpositionSolver::ClausePtr SuperpositionSolver::applyClauseSubsumption(
    const ClausePtr& subsumed, const ClausePtr& subsuming) const {
    if (!subsumed) return nullptr;
    assert(subsuming);
    if (subsumed->literals.size() < subsuming->literals.size()) {
        return subsumed;
    }

    std::function<bool(size_t, const Substitution&, const std::vector<bool>&)> checkRecursively =
        [&](size_t subsumingClauseIndex, const Substitution& currentSubstitution,
            const std::vector<bool>& mask) -> bool {
        if (subsumingClauseIndex == subsuming->literals.size()) return true;
        auto patternLiteral = subsuming->literals[subsumingClauseIndex];
        for (size_t i = 0; i < subsumed->literals.size(); ++i) {
            if (!mask[i]) continue;
            const auto& targetLiteral = subsumed->literals[i];
            auto nextMask = mask;
            nextMask[i] = false;
            auto substitutions = Unification::matchCommutative(
                patternLiteral, targetLiteral, currentSubstitution);
            for (const auto& nextSubstitution : substitutions) {
                if (checkRecursively(subsumingClauseIndex + 1, nextSubstitution, nextMask)) {
                    return true;
                }
            }
        }
        return false;
    };

    std::vector<bool> mask(subsumed->literals.size(), true);
    if (checkRecursively(0, Substitution{}, mask)) return nullptr;
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

    Literals newLiterals;
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
    return Clause::create(std::move(newLiterals),
        "positive_simplify-reflect", true, clause, unitClause);
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

    Literals newLiterals;
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
    return Clause::create(std::move(newLiterals),
        "negative_simplify-reflect", true, clause, unitClause);
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

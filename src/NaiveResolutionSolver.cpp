#include "NaiveResolutionSolver.hpp"

#include "ExpressionBuilder.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>

struct NaiveResolutionSolver::Clause {
    const ProofNodePtr input;
    const ClausePtr parent1;
    const ClausePtr parent2;

    std::vector<FormulaPtr> literals;

    Clause(ProofNodePtr input) : input(input), parent1(nullptr), parent2(nullptr) {}
    Clause(ClausePtr parent1, ClausePtr parent2 = nullptr) :
        input(nullptr), parent1(parent1), parent2(parent2) {
    }
};

void NaiveResolutionSolver::setTimeLimit(int seconds) {
    timeLimitSeconds = static_cast<double>(seconds);
}

void NaiveResolutionSolver::setMemoryLimit(int megabytes) {
    memoryLimitMegabytes = megabytes;
}

FolSatSolver::Result NaiveResolutionSolver::solve(const std::vector<ProofNodePtr>& clauses) {
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
        ClausePtr newClause = std::make_shared<Clause>(clauses[i]);
        newClause->literals = junction->operands;
        bool isTautology = removeBoolLiterals(newClause->literals);
        if (!isTautology) isTautology = handleDistinctObjects(newClause->literals);
        if (!isTautology) {
            if (newClause->literals.empty()) {
                proofRoot = newClause;
                return FolSatSolver::Result::UNSATISFIABLE;
            }
            standardizeVariables(newClause);
            unprocessedClauses.push_back(newClause);
        }
    }

    addEqualityAxioms(unprocessedClauses);

    while (!unprocessedClauses.empty()) {
        if (++iterationCounter > 1024) {
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

        std::vector<ClausePtr> tmpClauses;
        factor(unprocClause, tmpClauses);
        for (const auto& procClause : processedClauses) {
            resolve(procClause, unprocClause, tmpClauses);
        }

        for (auto& tmpClause : tmpClauses) {
            if (tmpClause->literals.empty()) {
                proofRoot = tmpClause;
                return FolSatSolver::Result::UNSATISFIABLE;
            }
            standardizeVariables(tmpClause);
            unprocessedClauses.push_back(tmpClause);
        }
        processedClauses.push_back(unprocClause);
    }
    return FolSatSolver::Result::SATISFIABLE;
}

ProofNodePtr NaiveResolutionSolver::getProof() const {
    if (!proofRoot) return nullptr;
    std::map<ClausePtr, ProofNodePtr> cache;
    return reconstructProof(proofRoot, cache);
}

bool NaiveResolutionSolver::removeBoolLiterals(std::vector<FormulaPtr>& literals) {
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
        if (remove) it = literals.erase(it);
        else ++it;
    }
    return false; // not tautology
}

void NaiveResolutionSolver::standardizeVariables(ClausePtr& clause) {
    if (clause->literals.empty()) return;
    auto junction = std::make_shared<JunctionFormula>(
        JunctionFormula::Operator::OR, clause->literals);
    auto standardized = transformer.standardizeVariables(junction, true);
    assert(standardized->exprType == Expression::Type::JUNCTION);
    auto standardizedJunction = std::static_pointer_cast<JunctionFormula>(standardized);
    clause->literals = standardizedJunction->operands;
}

void NaiveResolutionSolver::resolve(const ClausePtr& clause1, const ClausePtr& clause2,
    std::vector<ClausePtr>& resolvents) {
    for (size_t i1 = 0; i1 < clause1->literals.size(); ++i1) {
        auto literal1 = clause1->literals[i1];
        for (size_t i2 = 0; i2 < clause2->literals.size(); ++i2) {
            auto literal2 = clause2->literals[i2];

            bool isNegation1 = (literal1->exprType == Expression::Type::NEGATION);
            bool isNegation2 = (literal2->exprType == Expression::Type::NEGATION);
            if (isNegation1 == isNegation2) continue;
            FormulaPtr atom1, atom2;
            if (isNegation1) {
                auto negation = std::static_pointer_cast<NegationFormula>(literal1);
                assert(negation->child);
                atom1 = negation->child;
            }
            else {
                atom1 = literal1;
            }
            if (isNegation2) {
                auto negation = std::static_pointer_cast<NegationFormula>(literal2);
                assert(negation->child);
                atom2 = negation->child;
            }
            else {
                atom2 = literal2;
            }
            assert(atom1->isAtom() && atom2->isAtom());

            if (atom1->exprType != atom2->exprType) continue;
            Substitution mgu;
            if (unify(atom1, atom2, mgu)) {
                ClausePtr newClause = std::make_shared<Clause>(clause1, clause2);
                for (size_t i = 0; i < clause1->literals.size(); ++i) {
                    if (i != i1) {
                        auto newLiteral = substitute(clause1->literals[i], mgu);
                        newClause->literals.push_back(std::static_pointer_cast<Formula>(newLiteral));
                    }
                }
                for (size_t i = 0; i < clause2->literals.size(); ++i) {
                    if (i != i2) {
                        auto newLiteral = substitute(clause2->literals[i], mgu);
                        newClause->literals.push_back(std::static_pointer_cast<Formula>(newLiteral));
                    }
                }
                bool isTautology = handleDistinctObjects(newClause->literals);
                if (!isTautology) resolvents.push_back(newClause);
            }
        }
    }
}

void NaiveResolutionSolver::factor(const ClausePtr& clause, std::vector<ClausePtr>& factors) {
    for (size_t i1 = 0; i1 < clause->literals.size(); ++i1) {
        auto literal1 = clause->literals[i1];
        for (size_t i2 = i1 + 1; i2 < clause->literals.size(); ++i2) {
            auto literal2 = clause->literals[i2];

            bool isNegation1 = (literal1->exprType == Expression::Type::NEGATION);
            bool isNegation2 = (literal2->exprType == Expression::Type::NEGATION);
            if (isNegation1 != isNegation2) continue;
            FormulaPtr atom1, atom2;
            if (isNegation1) {
                auto negation = std::static_pointer_cast<NegationFormula>(literal1);
                assert(negation->child);
                atom1 = negation->child;
            }
            else {
                atom1 = literal1;
            }
            if (isNegation2) {
                auto negation = std::static_pointer_cast<NegationFormula>(literal2);
                assert(negation->child);
                atom2 = negation->child;
            }
            else {
                atom2 = literal2;
            }
            assert(atom1->isAtom() && atom2->isAtom());

            Substitution mgu;
            if (unify(atom1, atom2, mgu)) {
                ClausePtr newClause = std::make_shared<Clause>(clause);
                for (size_t i = 0; i < clause->literals.size(); ++i) {
                    if (i != i1) {
                        auto newLiteral = substitute(clause->literals[i], mgu);
                        newClause->literals.push_back(std::static_pointer_cast<Formula>(newLiteral));
                    }
                }
                bool isTautology = handleDistinctObjects(newClause->literals);
                if (!isTautology) factors.push_back(newClause);
            }
        }
    }
}

bool NaiveResolutionSolver::unify(const ExpressionPtr& expr1, const ExpressionPtr& expr2,
    Substitution& mgu) {
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
                if (occursCheck(variable->symbol, term, mgu)) return false;
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

bool NaiveResolutionSolver::occursCheck(const std::string& symbol,
    const ExpressionPtr& expr, const Substitution& mgu) {
    assert(expr && !symbol.empty());
    if (!expr || symbol.empty()) return false;

    if (expr->exprType == Expression::Type::VARIABLE) {
        auto variable = std::static_pointer_cast<VariableTerm>(expr);
        assert(!variable->symbol.empty());
        if (variable->symbol == symbol) return true;
        auto it = mgu.find(variable->symbol);
        if (it != mgu.end()) {
            return occursCheck(symbol, it->second, mgu);
        }
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child = expr->getChild(i);
        if (occursCheck(symbol, child, mgu)) return true;
    }
    return false;
}

ExpressionPtr NaiveResolutionSolver::substitute(const ExpressionPtr& expr,
    const Substitution& substitution, bool inPlace) {
    assert(expr);
    if (!expr) return nullptr;
    if (!inPlace) {
        return substitute(expr->clone(), substitution, true);
    }

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

void NaiveResolutionSolver::addEqualityAxioms(std::deque<ClausePtr>& clauses) {
    namespace EB = ExpressionBuilder;

    std::vector<ExpressionPtr> stack;
    bool containsEquality = false;
    for (const auto& clause : clauses) {
        for (const auto& literal : clause->literals) {
            stack.push_back(literal);
            if (literal->exprType == Expression::Type::EQUALITY) {
                containsEquality = true;
            }
            else if (literal->exprType == Expression::Type::NEGATION &&
                literal->getChild(0)->exprType == Expression::Type::EQUALITY) {
                containsEquality = true;
            }
        }
    }
    if (!containsEquality) return;

    std::map<std::string, size_t> funcArities;
    std::map<std::string, size_t> predArities;
    while (!stack.empty()) {
        auto expr = stack.back(); stack.pop_back();
        if (!expr) continue;
        if (expr->exprType == Expression::Type::FUNCTION) {
            auto func = std::static_pointer_cast<FunctionTerm>(expr);
            auto [it, inserted] = funcArities.try_emplace(func->symbol, func->arguments.size());
            assert(it->second == func->arguments.size());
        }
        else if (expr->exprType == Expression::Type::PREDICATE) {
            auto pred = std::static_pointer_cast<PredicateFormula>(expr);
            auto [it, inserted] = predArities.try_emplace(pred->symbol, pred->arguments.size());
            assert(it->second == pred->arguments.size());
        }
        size_t count = expr->getChildCount();
        for (size_t i = 0; i < count; ++i) {
            stack.push_back(expr->getChild(i));
        }
    }

    auto addAxiom = [&](std::vector<FormulaPtr> literals) {
        FormulaPtr formula = (literals.size() == 1) ? literals[0] : EB::Disjunction(literals);
        auto proofNode = ProofStep::create(formula, ProofNode::Type::PREMISE, "equality_axiom", {});
        auto clause = std::make_shared<Clause>(proofNode);
        clause->literals = std::move(literals);
        standardizeVariables(clause);
        clauses.push_back(clause);
        };

    // Reflexivity: X = X
    addAxiom({ EB::Equal(EB::Var("X"), EB::Var("X")) });
    // Symmetry: X = Y => Y = X   (CNF: ~(X=Y) | Y=X)
    addAxiom({
        EB::Not(EB::Equal(EB::Var("X"), EB::Var("Y"))),
        EB::Equal(EB::Var("Y"), EB::Var("X"))
        });
    // Transitivity: X = Y & Y = Z => X = Z   (CNF: ~(X=Y) | ~(Y=Z) | X=Z)
    addAxiom({
        EB::Not(EB::Equal(EB::Var("X"), EB::Var("Y"))),
        EB::Not(EB::Equal(EB::Var("Y"), EB::Var("Z"))),
        EB::Equal(EB::Var("X"), EB::Var("Z"))
        });

    // Function Congruence: (X0=Y0) & (X1=Y1) & ... => f(X0, X1, ...) = f(Y0, Y1, ...)
    // CNF: ~Eq(X0, Y0) | ~Eq(X1, Y1) | ... | Eq(f(X0, X1, ...), f(Y0, Y1, ...))
    for (const auto& [name, arity] : funcArities) {
        if (arity == 0) continue;
        std::vector<FormulaPtr> literals; literals.reserve(arity + 1);
        std::vector<TermPtr> argsX, argsY; argsX.reserve(arity); argsY.reserve(arity);
        for (size_t i = 0; i < arity; ++i) {
            std::string sX = "X" + std::to_string(i);
            std::string sY = "Y" + std::to_string(i);
            argsX.push_back(EB::Var(sX)); argsY.push_back(EB::Var(sY));
            literals.push_back(EB::Not(EB::Equal(EB::Var(sX), EB::Var(sY))));
        }
        literals.push_back(EB::Equal(EB::Func(name, argsX), EB::Func(name, argsY)));
        addAxiom(std::move(literals));
    }

    // Predicate Congruence: (X0=Y0) & (X1=Y1) & ... & P(X0, X1...) => P(Y0, Y1, ...)
    // CNF: ~Eq(X0, Y0) | ~Eq(X1, Y1) | ... | ~P(X0, X1, ...) | P(Y0, Y1, ...)
    for (const auto& [name, arity] : predArities) {
        if (arity == 0) continue;
        std::vector<FormulaPtr> literals; literals.reserve(arity + 2);
        std::vector<TermPtr> argsX, argsY; argsX.reserve(arity); argsY.reserve(arity);
        for (size_t i = 0; i < arity; ++i) {
            std::string sX = "X" + std::to_string(i);
            std::string sY = "Y" + std::to_string(i);
            argsX.push_back(EB::Var(sX)); argsY.push_back(EB::Var(sY));
            literals.push_back(EB::Not(EB::Equal(EB::Var(sX), EB::Var(sY))));
        }
        literals.push_back(EB::Not(EB::Pred(name, argsX)));
        literals.push_back(EB::Pred(name, argsY));
        addAxiom(std::move(literals));
    }
}

ProofNodePtr NaiveResolutionSolver::reconstructProof(const ClausePtr& clause,
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
    else {
        formula = std::make_shared<JunctionFormula>(
            JunctionFormula::Operator::OR, clause->literals);
    }
    std::string rule = clause->parent2 ? "resolution" : "factoring";
    std::vector<ProofNodePtr> parents;
    if (clause->parent1) parents.push_back(reconstructProof(clause->parent1, cache));
    if (clause->parent2) parents.push_back(reconstructProof(clause->parent2, cache));

    auto node = ProofStep::create(std::move(formula), ProofNode::Type::INFERENCE,
        std::move(rule), std::move(parents));
    cache[clause] = node;
    return node;
}

bool NaiveResolutionSolver::handleDistinctObjects(std::vector<FormulaPtr>& literals) {
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
                continue;
            }
        }
        ++it;
    }
    return false;
}

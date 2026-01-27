#include "ExpressionTransformer.hpp"

#include "ExpressionRewriter.hpp"
#include "ExpressionUtils.hpp"

#include <cassert>

using namespace ExpressionUtils;

ExpressionTransformer::ExpressionTransformer(std::shared_ptr<NameRegistry> nameRegistry) :
    nameRegistry(nameRegistry ? nameRegistry : std::make_shared<NameRegistry>()) {
}

std::vector<FormulaPtr> ExpressionTransformer::toCnf(const FormulaPtr& formula) {
    using namespace ExpressionRewriter;
    using namespace ExpressionRewriter::DSL;

    assert(isDag(formula));
    assert(isFullyDefined(formula));
    assert(isArityConsistent(formula));

    nameRegistry->clearVariableNames();
    auto current = std::static_pointer_cast<Formula>(formula->clone());

    auto X = Variable("x");
    auto A = Metavariable("A");
    auto B = Metavariable("B");
    auto C = Metavariable("C");

    // Elimination of junctions
    current = eliminateJunction(current, true);

    // Elimination of implications, equivalences, and exclusive ORs
    std::vector<ReplacementRule> arrowRules = {
        // A => B  -->  ~A | B
        { Imp(A, B), Or(Not(A), B) },
        // A <=> B  -->  (~A | B) & (~B | A)
        { Eqv(A, B), And(Or(Not(A), B), Or(Not(B), A)) },
        // A <~> B  -->  (A | B) & (~A | ~B)
        { Xor(A, B), And(Or(A, B), Or(Not(A), Not(B))) }
    };
    current = rewriteFast(current, arrowRules, true);

    // Boolean simplification
    std::vector<ReplacementRule> boolRules = {
        // ~True --> False               // ~False --> True
        { Not(True()), False() },        { Not(False()), True() },
        // A & True --> A                // True & A --> A
        { And(A, True()), A },           { And(True(), A), A },
        // A & False --> False           // False & A --> False
        { And(A, False()), False() },    { And(False(), A), False() },
        // A | False --> A               // False | A --> A
        { Or(A, False()), A },           { Or(False(), A), A },
        // A | True --> True             // True | A --> True
        { Or(A, True()), True() },       { Or(True(), A), True() },
        // !X.True --> True              // !X.False --> False
        { Forall(X, True()), True() }, { Forall(X, False()), False() },
        // ?X.True --> True              // ?X.False --> False
        { Exists(X, True()), True() }, { Exists(X, False()), False() }
    };
    current = rewriteFast(current, boolRules, true);

    // Conversion to Negation Normal Form (NNF)
    std::vector<ReplacementRule> nnfRules = {
        // ~~A  -->  A
        { Not(Not(A)), A },
        // ~(A & B)  -->  ~A | ~B
        { Not(And(A, B)), Or(Not(A), Not(B)) },
        // ~(A | B)  -->  ~A & ~B
        { Not(Or(A, B)), And(Not(A), Not(B)) },
        // ~!X.A  -->  ?X.~A
        { Not(Forall(X, A)), Exists(X, Not(A)) },
        // ~?X.A  -->  !X.~A
        { Not(Exists(X, A)), Forall(X, Not(A)) }
    };
    current = rewriteFast(current, nnfRules, true);
    assert(isNnf(current));

    // Miniscoping
    std::vector<ReplacementRule> miniScopeRules = {
        // 1. Standard distributivity
        // !X.(A & B) --> (!X.A) & (!X.B)
        { Forall(X, And(A, B)), And(Forall(X, A), Forall(X, B)) },
        // ?X.(A | B) --> (?X.A) | (?X.B)
        { Exists(X, Or(A, B)),  Or(Exists(X, A), Exists(X, B))  },

        // 2. Scope extrusion (pushing quantifiers deeper)
        // !X.(A | B) --> A | (!X.B)      (condition: X is not free in A)
        { Forall(X, Or(A, B)), Or(A, Forall(X, B)), { NotFreeIn(X, A) } },
        // !X.(A | B) --> (!X.A) | B      (condition: X is not free in B)
        { Forall(X, Or(A, B)), Or(Forall(X, A), B), { NotFreeIn(X, B) } },
        // ?X.(A & B) --> A & (?X.B)      (condition: X is not free in A)
        { Exists(X, And(A, B)), And(A, Exists(X, B)), { NotFreeIn(X, A) } },
        // ?X.(A & B) --> (?X.A) & B      (condition: X is not free in B)
        { Exists(X, And(A, B)), And(Exists(X, A), B), { NotFreeIn(X, B) } },

        // 3. Vacuous quantification (removing unused quantifiers)
        // !X.A --> A                     (condition: X is not free in A)
        { Forall(X, A), A, { NotFreeIn(X, A) } },
        // ?X.A --> A                     (condition: X is not free in A)
        { Exists(X, A), A, { NotFreeIn(X, A) } }
    };
    current = rewriteFast(current, miniScopeRules, true);

    // Variable standardization
    if (!isStandardized(current)) {
        current = standardizeVariables(current, true);
    }

    // Skolemization
    current = skolemize(current, true);

    // Elimination of universal quantifiers
    current = eliminateQuantifiers(current, QuantificationFormula::Quantifier::FORALL, true);

    // Distribution of AND over OR (CNF) with Alpha-Equivalence optimizations
    auto A_prime = Metavariable("A_prime");
    std::vector<ReplacementRule> optDistRules = {
        // 1. Boolean simplification (Cleaning up constants)
        // A | True  -->  True
        { Or(A, True()), True() },
        // True | A  -->  True
        { Or(True(), A), True() },
        // A | False  -->  A
        { Or(A, False()), A },
        // False | A  -->  A
        { Or(False(), A), A },

        // 2. Tautology elimination (with Alpha-Equivalence)
        // A | ~A  -->  True
        { Or(A, Not(A_prime)), True(), { AreAlphaEquivalent(A, A_prime) } },
        // ~A | A  -->  True
        { Or(Not(A), A_prime), True(), { AreAlphaEquivalent(A, A_prime) } },

        // 3. Absorption Laws (The "Explosion Killers" with Alpha-Equivalence)
        // A | (A & B)  -->  A
        { Or(A, And(A_prime, B)), A, { AreAlphaEquivalent(A, A_prime) } },
        // (A & B) | A  -->  A
        { Or(And(A_prime, B), A), A, { AreAlphaEquivalent(A, A_prime) } },
        // A | (B & A)  -->  A
        { Or(A, And(B, A_prime)), A, { AreAlphaEquivalent(A, A_prime) } },
        // (B & A) | A  -->  A
        { Or(And(B, A_prime), A), A, { AreAlphaEquivalent(A, A_prime) } },

        // 4. Advanced Absorption (Optional but recommended)
        // A | (~A & B)  -->  A | B
        { Or(A, And(Not(A_prime), B)), Or(A, B), { AreAlphaEquivalent(A, A_prime) } },
        // (~A & B) | A  -->  B | A
        { Or(And(Not(A_prime), B), A), Or(B, A), { AreAlphaEquivalent(A, A_prime) } },

        // Consensus optimization (The "Shortcut")
        // (A & B) | (~A & C) --> (A | C) & (~A | B)
        {
            Or(And(A, B), And(Not(A_prime), C)),
            And(Or(A, C), Or(Not(A_prime), B)),
            { AreAlphaEquivalent(A, A_prime) }
        },
        // (~A & B) | (A & C) --> (~A | C) & (A | B)
        {
            Or(And(Not(A), B), And(A_prime, C)),
            And(Or(Not(A), C), Or(A_prime, B)),
            { AreAlphaEquivalent(A, A_prime) }
        },

        // 6. Proper Distribution (The actual CNF conversion)
        // A | (B & C)  -->  (A | B) & (A | C)
        { Or(A, And(B, C)), And(Or(A, B), Or(A, C)) },
        // (B & C) | A  -->  (B | A) & (C | A)
        { Or(And(B, C), A), And(Or(B, A), Or(C, A)) }
    };
    current = rewriteFast(current, optDistRules, true);

    // Flattening
    auto junction = flattenToJunction(current, JunctionFormula::Operator::AND, true);
    for (auto& clause : junction->operands) {
        clause = flattenToJunction(clause, JunctionFormula::Operator::OR, true);
    }
    assert(isJunctionCnf(junction));
    return junction->operands;
}

FormulaPtr ExpressionTransformer::eliminateJunction(const FormulaPtr& formula, bool inPlace) {
    assert(isDag(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    std::unordered_map<ExpressionPtr, ExpressionPtr> visited;
    return std::static_pointer_cast<Formula>(eliminateJunctionRec(f, visited));
}

FormulaPtr ExpressionTransformer::standardizeVariables(const FormulaPtr& formula, bool inPlace) {
    assert(isTree(formula) && isFullyDefined(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    std::unordered_map<std::string, std::string> nameMap;
    standardizeVariablesRec(f, nameMap);
    assert(isStandardized(f));
    return f;
}

FormulaPtr ExpressionTransformer::skolemize(const FormulaPtr& formula, bool inPlace) {
    assert(isNnf(formula));
    assert(isStandardized(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    auto exprToFreeVars = getFreeVariablesPerNode(f);
    std::unordered_set<std::string> scope;
    std::unordered_map<std::string, TermPtr> replacements;
    const auto& globalFreeVars = exprToFreeVars[f];
    scope.insert(globalFreeVars.begin(), globalFreeVars.end());
    auto result = skolemizeRec(f, scope, replacements, exprToFreeVars);
    return std::static_pointer_cast<Formula>(result);
}

FormulaPtr ExpressionTransformer::eliminateQuantifiers(const FormulaPtr& formula,
    QuantificationFormula::Quantifier typeToRemove, bool inPlace) {
    assert(isStandardized(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    auto processedFormula = eliminateQuantifiersRec(f, typeToRemove);
    return std::static_pointer_cast<Formula>(processedFormula);
}

JunctionFormulaPtr ExpressionTransformer::flattenToJunction(const FormulaPtr& formula,
    JunctionFormula::Operator targetOp, bool inPlace) {
    assert(isTree(formula) && isFullyDefined(formula));
    auto f = inPlace ? formula : std::static_pointer_cast<Formula>(formula->clone());
    std::vector<FormulaPtr> operands;
    collectOperandsRec(f, targetOp, operands);
    return std::make_shared<JunctionFormula>(targetOp, operands);
}

ExpressionPtr ExpressionTransformer::eliminateJunctionRec(const ExpressionPtr& expr,
    std::unordered_map<ExpressionPtr, ExpressionPtr>& visited) {
    if (!expr) return nullptr;
    if (auto it = visited.find(expr); it != visited.end()) return it->second;
    if (!expr->isFormula()) return expr;

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        auto child = expr->getChild(i);
        auto newChild = eliminateJunctionRec(child, visited);
        if (child != newChild) {
            expr->setChild(i, newChild);
        }
    }

    ExpressionPtr result = expr;
    if (expr->exprType == Expression::Type::JUNCTION) {
        auto junction = std::static_pointer_cast<JunctionFormula>(expr);
        const auto& operands = junction->operands;
        if (operands.empty()) {
            result = std::make_shared<BooleanFormula>(
                junction->op == JunctionFormula::Operator::AND);
        }
        else {
            auto binaryOp = (junction->op == JunctionFormula::Operator::AND)
                ? BinaryFormula::Operator::AND : BinaryFormula::Operator::OR;
            result = operands.front();
            for (size_t i = 1; i < operands.size(); ++i) {
                result = std::make_shared<BinaryFormula>(
                    binaryOp,
                    std::static_pointer_cast<Formula>(result),
                    std::static_pointer_cast<Formula>(operands[i])
                );
            }
        }
    }
    visited[expr] = result;
    return result;
}

void ExpressionTransformer::standardizeVariablesRec(const ExpressionPtr& expr,
    std::unordered_map<std::string, std::string>& nameMap) {
    assert(expr && "Expression pointer is null");
    if (!expr) return;

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        std::string oldName = quant->variable->symbol;
        assert(!oldName.empty());
        std::string newName = nameRegistry->getUniqueVariableName();
        quant->variable->symbol = newName;
        std::string backupName;
        if (auto it = nameMap.find(oldName); it != nameMap.end()) {
            backupName = it->second;
        }
        nameMap[oldName] = newName;
        standardizeVariablesRec(quant->body, nameMap);
        if (!backupName.empty()) nameMap[oldName] = backupName;
        else nameMap.erase(oldName);
        return;
    }

    if (expr->exprType == Expression::Type::VARIABLE) {
        auto var = std::static_pointer_cast<VariableTerm>(expr);
        assert(!var->symbol.empty());
        auto it = nameMap.find(var->symbol);
        if (it != nameMap.end()) {
            var->symbol = it->second;
        }
        else {
            std::string newName = nameRegistry->getUniqueVariableName();
            nameMap[var->symbol] = newName;
            var->symbol = newName;
        }
        return;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child = expr->getChild(i);
        standardizeVariablesRec(child, nameMap);
    }
}

ExpressionPtr ExpressionTransformer::skolemizeRec(const ExpressionPtr& expr,
    std::unordered_set<std::string>& universalVars,
    std::unordered_map<std::string, TermPtr>& substitutions,
    std::unordered_map<ExpressionPtr, std::set<std::string>>& exprToFreeVars) {
    assert(expr && "Expression pointer is null");
    if (!expr) return nullptr;

    if (expr->exprType == Expression::Type::FUNCTION) {
        auto func = std::static_pointer_cast<FunctionTerm>(expr);
        if (!func->distinct) {
            // sanity check: guard against missing symbol registration
            // make sure all symbols are registred before skolemization
            assert(nameRegistry->isFunctionNameRegistered(func->symbol));
        }
    }

    if (expr->exprType == Expression::Type::VARIABLE) {
        auto var = std::static_pointer_cast<VariableTerm>(expr);
        auto it = substitutions.find(var->symbol);
        if (it != substitutions.end()) {
            return it->second->clone();
        }
        return expr;
    }

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);

        if (quant->type == QuantificationFormula::Quantifier::FORALL) {
            std::string varName = quant->variable->symbol;
            universalVars.insert(varName);
            ExpressionPtr newBody = skolemizeRec(quant->body, universalVars, substitutions, exprToFreeVars);
            quant->body = std::static_pointer_cast<Formula>(newBody);
            universalVars.erase(varName);
            return quant;
        }
        else {
            std::vector<TermPtr> skolemArgs;
            const auto& freeVarNames = exprToFreeVars.at(quant->body);
            for (const auto& freeVarName : freeVarNames) {
                if (universalVars.find(freeVarName) != universalVars.end()) {
                    skolemArgs.push_back(std::make_shared<VariableTerm>(freeVarName));
                }
            }
            auto skolemTerm = std::make_shared<FunctionTerm>(nameRegistry->getUniqueFunctionName(), skolemArgs);
            substitutions[quant->variable->symbol] = skolemTerm;
            ExpressionPtr result = skolemizeRec(quant->body, universalVars, substitutions, exprToFreeVars);
            substitutions.erase(quant->variable->symbol);
            return result;
        }
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child = expr->getChild(i);
        ExpressionPtr newChild = skolemizeRec(child, universalVars, substitutions, exprToFreeVars);
        if (child != newChild) {
            expr->setChild(i, newChild);
        }
    }
    return expr;
}

ExpressionPtr ExpressionTransformer::eliminateQuantifiersRec(const ExpressionPtr& expr,
    QuantificationFormula::Quantifier typeToRemove) {
    assert(expr && "Expression pointer is null");
    if (!expr) return nullptr;

    if (expr->exprType == Expression::Type::QUANTIFICATION) {
        auto quant = std::static_pointer_cast<QuantificationFormula>(expr);
        ExpressionPtr processedBody = eliminateQuantifiersRec(quant->body, typeToRemove);
        if (quant->type == typeToRemove) {
            return processedBody;
        }
        quant->body = std::static_pointer_cast<Formula>(processedBody);
        return quant;
    }

    if (expr->isFormula()) {
        if (std::static_pointer_cast<Formula>(expr)->isAtom()) return expr;
    }

    size_t count = expr->getChildCount();
    for (size_t i = 0; i < count; ++i) {
        ExpressionPtr child = expr->getChild(i);
        ExpressionPtr newChild = eliminateQuantifiersRec(child, typeToRemove);
        if (child != newChild) {
            expr->setChild(i, newChild);
        }
    }
    return expr;
}

void ExpressionTransformer::collectOperandsRec(const FormulaPtr& formula,
    JunctionFormula::Operator targetOp, std::vector<FormulaPtr>& accumulator) {
    assert(formula && "Formula pointer is null");
    if (!formula) return;

    if (formula->exprType == Expression::Type::BINARY) {
        auto binary = std::static_pointer_cast<BinaryFormula>(formula);
        if ((targetOp == JunctionFormula::Operator::AND && binary->op == BinaryFormula::Operator::AND) ||
            (targetOp == JunctionFormula::Operator::OR && binary->op == BinaryFormula::Operator::OR)) {
            collectOperandsRec(binary->left, targetOp, accumulator);
            collectOperandsRec(binary->right, targetOp, accumulator);
            return;
        }
    }

    if (formula->exprType == Expression::Type::JUNCTION) {
        auto junction = std::static_pointer_cast<JunctionFormula>(formula);
        if (junction->op == targetOp) {
            for (const auto& op : junction->operands) {
                collectOperandsRec(op, targetOp, accumulator);
            }
            return;
        }
    }

    accumulator.push_back(formula);
}

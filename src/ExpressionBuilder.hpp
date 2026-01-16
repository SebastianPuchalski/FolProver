#pragma once

#include "Expression.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ExpressionBuilder {

    // Null / Empty Node
    inline ExpressionPtr Null() {
        return nullptr;
    }

    // Boolean Constants
    inline BooleanFormulaPtr True() {
        return std::make_shared<BooleanFormula>(true);
    }
    inline BooleanFormulaPtr False() {
        return std::make_shared<BooleanFormula>(false);
    }

    // Negation
    inline NegationFormulaPtr Not(FormulaPtr child) {
        return std::make_shared<NegationFormula>(std::move(child));
    }

    // Binary Operators
    inline BinaryFormulaPtr And(FormulaPtr left, FormulaPtr right) {
        return std::make_shared<BinaryFormula>(BinaryFormula::Operator::AND, std::move(left), std::move(right));
    }
    inline BinaryFormulaPtr Or(FormulaPtr left, FormulaPtr right) {
        return std::make_shared<BinaryFormula>(BinaryFormula::Operator::OR, std::move(left), std::move(right));
    }
    inline BinaryFormulaPtr Imp(FormulaPtr left, FormulaPtr right) {
        return std::make_shared<BinaryFormula>(BinaryFormula::Operator::IMP, std::move(left), std::move(right));
    }
    inline BinaryFormulaPtr Eqv(FormulaPtr left, FormulaPtr right) {
        return std::make_shared<BinaryFormula>(BinaryFormula::Operator::EQV, std::move(left), std::move(right));
    }
    inline BinaryFormulaPtr Xor(FormulaPtr left, FormulaPtr right) {
        return std::make_shared<BinaryFormula>(BinaryFormula::Operator::XOR, std::move(left), std::move(right));
    }

    // N-ary Junctions
    inline JunctionFormulaPtr Conjunction(std::vector<FormulaPtr> operands) {
        return std::make_shared<JunctionFormula>(JunctionFormula::Operator::AND, std::move(operands));
    }
    inline JunctionFormulaPtr Disjunction(std::vector<FormulaPtr> operands) {
        return std::make_shared<JunctionFormula>(JunctionFormula::Operator::OR, std::move(operands));
    }

    // Quantification
    inline QuantificationFormulaPtr Forall(VariableTermPtr variable, FormulaPtr body) {
        return std::make_shared<QuantificationFormula>(QuantificationFormula::Quantifier::FORALL, std::move(variable), std::move(body));
    }
    inline QuantificationFormulaPtr Exists(VariableTermPtr variable, FormulaPtr body) {
        return std::make_shared<QuantificationFormula>(QuantificationFormula::Quantifier::EXISTS, std::move(variable), std::move(body));
    }

    // Predicate & Equality (Atoms)
    inline PredicateFormulaPtr Pred(std::string symbol, std::vector<TermPtr> arguments = {}) {
        return std::make_shared<PredicateFormula>(std::move(symbol), std::move(arguments));
    }
    inline EqualityFormulaPtr Equal(TermPtr left, TermPtr right) {
        return std::make_shared<EqualityFormula>(std::move(left), std::move(right));
    }

    // Terms
    inline FunctionTermPtr Func(std::string symbol, std::vector<TermPtr> arguments = {}) {
        return std::make_shared<FunctionTerm>(std::move(symbol), std::move(arguments));
    }
    inline FunctionTermPtr Distinct(std::string symbol) {
        return std::make_shared<FunctionTerm>(std::move(symbol), std::vector<TermPtr>{}, true);
    }
    inline VariableTermPtr Var(std::string symbol) {
        return std::make_shared<VariableTerm>(std::move(symbol));
    }

} // namespace ExpressionBuilder

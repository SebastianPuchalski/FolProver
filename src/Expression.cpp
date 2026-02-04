#include "Expression.hpp"

#include <cassert>

namespace {
    template <typename T>
    std::vector<std::shared_ptr<T>> cloneVector(const std::vector<std::shared_ptr<T>>& src) {
        std::vector<std::shared_ptr<T>> dest;
        dest.reserve(src.size());
        for (const auto& item : src) {
            if (item) {
                dest.push_back(std::static_pointer_cast<T>(item->clone()));
            }
            else {
                dest.push_back(nullptr);
            }
        }
        return dest;
    }
}

ExpressionPtr BooleanFormula::clone() const {
    return std::make_shared<BooleanFormula>(value);
}

ExpressionPtr NegationFormula::clone() const {
    return std::make_shared<NegationFormula>(
        child ? std::static_pointer_cast<Formula>(child->clone()) : nullptr
    );
}

ExpressionPtr BinaryFormula::clone() const {
    return std::make_shared<BinaryFormula>(
        op,
        left ? std::static_pointer_cast<Formula>(left->clone()) : nullptr,
        right ? std::static_pointer_cast<Formula>(right->clone()) : nullptr
    );
}

ExpressionPtr JunctionFormula::clone() const {
    return std::make_shared<JunctionFormula>(op, cloneVector(operands));
}

ExpressionPtr QuantificationFormula::clone() const {
    return std::make_shared<QuantificationFormula>(
        type,
        variable ? std::static_pointer_cast<VariableTerm>(variable->clone()) : nullptr,
        body ? std::static_pointer_cast<Formula>(body->clone()) : nullptr
    );
}

ExpressionPtr PredicateFormula::clone() const {
    return std::make_shared<PredicateFormula>(symbol, cloneVector(arguments));
}

ExpressionPtr EqualityFormula::clone() const {
    return std::make_shared<EqualityFormula>(
        left ? std::static_pointer_cast<Term>(left->clone()) : nullptr,
        right ? std::static_pointer_cast<Term>(right->clone()) : nullptr
    );
}

ExpressionPtr FunctionTerm::clone() const {
    return std::make_shared<FunctionTerm>(symbol, cloneVector(arguments), distinct);
}

ExpressionPtr VariableTerm::clone() const {
    return std::make_shared<VariableTerm>(symbol);
}

ExpressionPtr BooleanFormula::cloneShallow() const {
    return std::make_shared<BooleanFormula>(*this);
}

ExpressionPtr NegationFormula::cloneShallow() const {
    return std::make_shared<NegationFormula>(*this);
}

ExpressionPtr BinaryFormula::cloneShallow() const {
    return std::make_shared<BinaryFormula>(*this);
}

ExpressionPtr JunctionFormula::cloneShallow() const {
    return std::make_shared<JunctionFormula>(*this);
}

ExpressionPtr QuantificationFormula::cloneShallow() const {
    return std::make_shared<QuantificationFormula>(*this);
}

ExpressionPtr PredicateFormula::cloneShallow() const {
    return std::make_shared<PredicateFormula>(*this);
}

ExpressionPtr EqualityFormula::cloneShallow() const {
    return std::make_shared<EqualityFormula>(*this);
}

ExpressionPtr FunctionTerm::cloneShallow() const {
    return std::make_shared<FunctionTerm>(*this);
}

ExpressionPtr VariableTerm::cloneShallow() const {
    return std::make_shared<VariableTerm>(*this);
}

size_t BooleanFormula::getChildCount() const { return 0; }
size_t NegationFormula::getChildCount() const { return 1; }
size_t BinaryFormula::getChildCount() const { return 2; }
size_t JunctionFormula::getChildCount() const { return operands.size(); }
size_t QuantificationFormula::getChildCount() const { return 1; }
size_t PredicateFormula::getChildCount() const { return arguments.size(); }
size_t EqualityFormula::getChildCount() const { return 2; }
size_t FunctionTerm::getChildCount() const { return arguments.size(); }
size_t VariableTerm::getChildCount() const { return 0; }

ExpressionPtr BooleanFormula::getChild(size_t index) const {
    assert(false);
    return nullptr;
}

ExpressionPtr NegationFormula::getChild(size_t index) const {
    assert(index == 0);
    return child;
}

ExpressionPtr BinaryFormula::getChild(size_t index) const {
    assert(index < 2);
    return (index == 0) ? left : right;
}

ExpressionPtr JunctionFormula::getChild(size_t index) const {
    assert(index < operands.size());
    return operands[index];
}

ExpressionPtr QuantificationFormula::getChild(size_t index) const {
    assert(index == 0);
    return body;
}

ExpressionPtr PredicateFormula::getChild(size_t index) const {
    assert(index < arguments.size());
    return arguments[index];
}

ExpressionPtr EqualityFormula::getChild(size_t index) const {
    assert(index < 2);
    return (index == 0) ? left : right;
}

ExpressionPtr FunctionTerm::getChild(size_t index) const {
    assert(index < arguments.size());
    return arguments[index];
}

ExpressionPtr VariableTerm::getChild(size_t index) const {
    assert(false);
    return nullptr;
}

void BooleanFormula::setChild(size_t index, const ExpressionPtr& newChild) {
    assert(false);
}

void NegationFormula::setChild(size_t index, const ExpressionPtr& newChild) {
    assert(index == 0);
    assert(!newChild || newChild->isFormula());
    if (newChild && !newChild->isFormula()) {
        child = nullptr;
        return;
    }
    child = std::static_pointer_cast<Formula>(newChild);
}

void BinaryFormula::setChild(size_t index, const ExpressionPtr& newChild) {
    assert(index < 2);
    assert(!newChild || newChild->isFormula());
    if (newChild && !newChild->isFormula()) {
        if (index == 0) left = nullptr;
        else right = nullptr;
        return;
    }
    if (index == 0) left = std::static_pointer_cast<Formula>(newChild);
    else right = std::static_pointer_cast<Formula>(newChild);
}

void JunctionFormula::setChild(size_t index, const ExpressionPtr& newChild) {
    assert(index < operands.size());
    assert(!newChild || newChild->isFormula());
    if (newChild && !newChild->isFormula()) {
        operands[index] = nullptr;
        return;
    }
    operands[index] = std::static_pointer_cast<Formula>(newChild);
}

void QuantificationFormula::setChild(size_t index, const ExpressionPtr& newChild) {
    assert(index == 0);
    assert(!newChild || newChild->isFormula());
    if (newChild && !newChild->isFormula()) {
        body = nullptr;
        return;
    }
    body = std::static_pointer_cast<Formula>(newChild);
}

void PredicateFormula::setChild(size_t index, const ExpressionPtr& newChild) {
    assert(index < arguments.size());
    assert(!newChild || newChild->isTerm());
    if (newChild && !newChild->isTerm()) {
        arguments[index] = nullptr;
        return;
    }
    arguments[index] = std::static_pointer_cast<Term>(newChild);
}

void EqualityFormula::setChild(size_t index, const ExpressionPtr& newChild) {
    assert(index < 2);
    assert(!newChild || newChild->isTerm());
    if (newChild && !newChild->isTerm()) {
        if (index == 0) left = nullptr;
        else right = nullptr;
        return;
    }
    if (index == 0) left = std::static_pointer_cast<Term>(newChild);
    else right = std::static_pointer_cast<Term>(newChild);
}

void FunctionTerm::setChild(size_t index, const ExpressionPtr& newChild) {
    assert(index < arguments.size());
    assert(!newChild || newChild->isTerm());
    if (newChild && !newChild->isTerm()) {
        arguments[index] = nullptr;
        return;
    }
    arguments[index] = std::static_pointer_cast<Term>(newChild);
}

void VariableTerm::setChild(size_t index, const ExpressionPtr& newChild) {
    assert(false);
}

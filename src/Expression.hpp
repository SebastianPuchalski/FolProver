#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cassert>

struct Expression;
struct Formula;
struct BooleanFormula;
struct NegationFormula;
struct BinaryFormula;
struct JunctionFormula;
struct QuantificationFormula;
struct PredicateFormula;
struct EqualityFormula;
struct Term;
struct FunctionTerm;
struct VariableTerm;

using ExpressionPtr = std::shared_ptr<Expression>;
using FormulaPtr = std::shared_ptr<Formula>;
using BooleanFormulaPtr = std::shared_ptr<BooleanFormula>;
using NegationFormulaPtr = std::shared_ptr<NegationFormula>;
using BinaryFormulaPtr = std::shared_ptr<BinaryFormula>;
using JunctionFormulaPtr = std::shared_ptr<JunctionFormula>;
using QuantificationFormulaPtr = std::shared_ptr<QuantificationFormula>;
using PredicateFormulaPtr = std::shared_ptr<PredicateFormula>;
using EqualityFormulaPtr = std::shared_ptr<EqualityFormula>;
using TermPtr = std::shared_ptr<Term>;
using FunctionTermPtr = std::shared_ptr<FunctionTerm>;
using VariableTermPtr = std::shared_ptr<VariableTerm>;

struct Expression {
    enum class Type {
        BOOLEAN,
        NEGATION,
        BINARY,
        JUNCTION,
        QUANTIFICATION,
        PREDICATE,
        EQUALITY,
        FUNCTION,
        VARIABLE
    };

    const Type exprType;

    virtual ~Expression() = default;

    virtual bool isTerm() const = 0;
    virtual bool isFormula() const = 0;

    virtual std::shared_ptr<Expression> clone() const = 0;
    
    virtual size_t getChildCount() const = 0;
    virtual ExpressionPtr getChild(size_t index) const = 0;
    ExpressionPtr operator[](size_t index) const { return getChild(index); }
    virtual void setChild(size_t index, const ExpressionPtr& newChild) = 0;

protected:
    explicit Expression(Type t) : exprType(t) {}
};

// Formulas:

struct Formula : public Expression {
    virtual ~Formula() = default;

    bool isTerm() const override { return false; }
    bool isFormula() const override { return true; }

    virtual bool isAtom() const = 0;
    virtual bool isLiteral() const = 0;

protected:
    explicit Formula(Type t) : Expression(t) {}
};

struct BooleanFormula : public Formula {
    bool value;

    explicit BooleanFormula(bool value)
        : Formula(Type::BOOLEAN), value(value) {
    }

    virtual ~BooleanFormula() = default;

    // It's not clear whether boolean should be considered an atom or not
    bool isAtom() const override { return true; }
    bool isLiteral() const override { return true; }

    ExpressionPtr clone() const override;

    virtual size_t getChildCount() const override;
    virtual ExpressionPtr getChild(size_t index) const override;
    virtual void setChild(size_t index, const ExpressionPtr& newChild) override;
};

struct NegationFormula : public Formula {
    FormulaPtr child;

    explicit NegationFormula(FormulaPtr child = nullptr)
        : Formula(Type::NEGATION), child(std::move(child)) {
    }
    virtual ~NegationFormula() = default;

    bool isAtom() const override { return false; }
    bool isLiteral() const override { return child && child->isAtom(); }

    ExpressionPtr clone() const override;

    virtual size_t getChildCount() const override;
    virtual ExpressionPtr getChild(size_t index) const override;
    virtual void setChild(size_t index, const ExpressionPtr& newChild) override;
};

struct BinaryFormula : public Formula {
    enum class Operator { AND, OR, IMP, EQV, XOR };

    Operator op;
    FormulaPtr left;
    FormulaPtr right;

    explicit BinaryFormula(Operator op, FormulaPtr left = nullptr, FormulaPtr right = nullptr)
        : Formula(Type::BINARY), op(op), left(std::move(left)), right(std::move(right)) {
    }
    virtual ~BinaryFormula() = default;

    bool isAtom() const override { return false; }
    bool isLiteral() const override { return false; }

    ExpressionPtr clone() const override;

    virtual size_t getChildCount() const override;
    virtual ExpressionPtr getChild(size_t index) const override;
    virtual void setChild(size_t index, const ExpressionPtr& newChild) override;
};

struct JunctionFormula : public Formula {
    enum class Operator { AND, OR };

    Operator op;
    std::vector<FormulaPtr> operands;

    explicit JunctionFormula(Operator op, std::vector<FormulaPtr> operands = {})
        : Formula(Type::JUNCTION), op(op), operands(std::move(operands)) {
    }
    virtual ~JunctionFormula() = default;

    bool isAtom() const override { return false; }
    bool isLiteral() const override { return false; }

    ExpressionPtr clone() const override;

    virtual size_t getChildCount() const override;
    virtual ExpressionPtr getChild(size_t index) const override;
    virtual void setChild(size_t index, const ExpressionPtr& newChild) override;
};

struct QuantificationFormula : public Formula {
    enum class Quantifier { FORALL, EXISTS };

    Quantifier type;
    VariableTermPtr variable;
    FormulaPtr body;

    explicit QuantificationFormula(Quantifier type, VariableTermPtr variable = nullptr, FormulaPtr body = nullptr)
        : Formula(Type::QUANTIFICATION), type(type), variable(std::move(variable)), body(std::move(body)) {
    }
    virtual ~QuantificationFormula() = default;

    bool isAtom() const override { return false; }
    bool isLiteral() const override { return false; }

    ExpressionPtr clone() const override;

    virtual size_t getChildCount() const override;
    virtual ExpressionPtr getChild(size_t index) const override;
    virtual void setChild(size_t index, const ExpressionPtr& newChild) override;
};

struct PredicateFormula : public Formula {
    std::string symbol;
    std::vector<TermPtr> arguments;

    explicit PredicateFormula(std::string symbol, std::vector<TermPtr> arguments = {})
        : Formula(Type::PREDICATE), symbol(std::move(symbol)), arguments(std::move(arguments)) {
    }
    virtual ~PredicateFormula() = default;

    bool isAtom() const override { return true; }
    bool isLiteral() const override { return true; }

    ExpressionPtr clone() const override;

    virtual size_t getChildCount() const override;
    virtual ExpressionPtr getChild(size_t index) const override;
    virtual void setChild(size_t index, const ExpressionPtr& newChild) override;
};

struct EqualityFormula : public Formula {
    TermPtr left;
    TermPtr right;

    explicit EqualityFormula(TermPtr left, TermPtr right)
        : Formula(Type::EQUALITY), left(std::move(left)), right(std::move(right)) {
    }
    virtual ~EqualityFormula() = default;

    bool isAtom() const override { return true; }
    bool isLiteral() const override { return true; }

    ExpressionPtr clone() const override;

    virtual size_t getChildCount() const override;
    virtual ExpressionPtr getChild(size_t index) const override;
    virtual void setChild(size_t index, const ExpressionPtr& newChild) override;
};

// Terms:

struct Term : public Expression {
    virtual ~Term() = default;

    bool isTerm() const override { return true; }
    bool isFormula() const override { return false; }

protected:
    explicit Term(Type t) : Expression(t) {}
};

struct FunctionTerm : public Term {
    std::string symbol;
    std::vector<TermPtr> arguments;
    bool distinct;

    explicit FunctionTerm(std::string symbol,
        std::vector<TermPtr> arguments = {}, bool distinct = false) :
        Term(Type::FUNCTION), symbol(std::move(symbol)),
        arguments(std::move(arguments)), distinct(distinct) {
        assert(!distinct || this->arguments.empty());
    }
    virtual ~FunctionTerm() = default;

    ExpressionPtr clone() const override;

    virtual size_t getChildCount() const override;
    virtual ExpressionPtr getChild(size_t index) const override;
    virtual void setChild(size_t index, const ExpressionPtr& newChild) override;
};

struct VariableTerm : public Term {
    std::string symbol;

    explicit VariableTerm(std::string symbol)
        : Term(Type::VARIABLE), symbol(std::move(symbol)) {
    }
    virtual ~VariableTerm() = default;

    ExpressionPtr clone() const override;

    virtual size_t getChildCount() const override;
    virtual ExpressionPtr getChild(size_t index) const override;
    virtual void setChild(size_t index, const ExpressionPtr& newChild) override;
};

#pragma once

#include "Expression.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

class ProofNode;
class ProofStep;

using ProofNodePtr = std::shared_ptr<ProofNode>;
using ProofStepPtr = std::shared_ptr<ProofStep>;

class ProofNode {
public:
    enum class Type {
        PREMISE,
        CONJECTURE,
        NEGATED_CONJECTURE,
        INFERENCE
    };

    virtual ~ProofNode() = default;

    const FormulaPtr& getFormula() const { return formula; }
    Type getType() const { return type; }

    virtual bool isLeaf() const = 0;

protected:
    ProofNode(FormulaPtr formula, Type type) :
        formula(std::move(formula)), type(type) {
    }

    const FormulaPtr formula;
    const Type type;
};

class ProofStep : public ProofNode {
public:
    using Parents = std::vector<ProofNodePtr>;

    static ProofStepPtr create(FormulaPtr formula, Type type,
        std::string rule, Parents parents) {
        return std::make_shared<ProofStep>(std::move(formula), type,
            std::move(rule), std::move(parents));
    }

    ProofStep(FormulaPtr formula, Type type, std::string rule, Parents parents) :
        ProofNode(std::move(formula), type),
        rule(std::move(rule)),
        parents(std::move(parents)) {
    }

    bool isLeaf() const override { return false; }

    const std::string& getRule() const { return rule; }
    const Parents& getParents() const { return parents; }

private:
    const std::string rule;
    const Parents parents;
};

#pragma once

#include "../ProofNode.hpp"

#include <memory>
#include <string>
#include <utility>

class TptpProofNode;
using TptpProofNodePtr = std::shared_ptr<TptpProofNode>;

class TptpProofNode : public ProofNode {
public:
    static TptpProofNodePtr create(FormulaPtr formula, Type type,
        std::string name, std::string sourceFile, std::string role) {
        return std::make_shared<TptpProofNode>(std::move(formula), type,
            std::move(name), std::move(sourceFile), std::move(role));
    }

    TptpProofNode(FormulaPtr formula, Type type,
        std::string name, std::string sourceFile, std::string role) :
        ProofNode(std::move(formula), type),
        name(std::move(name)),
        sourceFile(std::move(sourceFile)),
        role(std::move(role)) {
    }

    const std::string& getName() const { return name; }
    const std::string& getSourceFile() const { return sourceFile; }
    const std::string& getRole() const { return role; }

    bool isLeaf() const override { return true; }

private:
    const std::string name;
    const std::string sourceFile;
    const std::string role;
};

#pragma once

#include "../ExpressionPrinter.hpp"
#include "../ExpressionTransformer.hpp"
#include "../ProofNode.hpp"

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace TptpTool {

class ProofPrinter {
public:
    enum class Format {
        TSTP,
        HTML
    };

    explicit ProofPrinter(Format format = Format::TSTP);

    std::string toString(const ProofNodePtr& proofRoot);

private:
    const Format format;
    const ExpressionPrinter exprPrinter;
    const ExpressionTransformer exprTransformer;

    std::unordered_map<const ProofNode*, std::string> nodeIds;
    std::unordered_set<std::string> reservedIds;
    int idCounter;

    std::ostringstream outputBuffer;

    void processOnlyTptpNodes(const ProofNodePtr& node,
        std::unordered_set<const ProofNode*>& visited);
    void processNodeRecursively(const ProofNodePtr& node);

    void nodeToTstp(const ProofNodePtr& node, std::string role);
    void nodeToHtml(const ProofNodePtr& node, std::string role);

    std::string getHtmlPrefix() const;
    std::string getHtmlSuffix() const;
};

} // namespace TptpTool

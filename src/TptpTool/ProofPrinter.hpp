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
        TEXT_UTF8,
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
    int idCounter;

    std::ostringstream outputBuffer;
    std::vector<std::vector<std::string>> textOutputBuffer;

    void processOnlyTptpNodes(const ProofNodePtr& node,
        std::unordered_set<const ProofNode*>& visited);
    void processNodeRecursively(const ProofNodePtr& node);

    void processTextOutputBuffer();

    void nodeToTextUtf8(const ProofNodePtr& node, std::string role);
    void nodeToTstp(const ProofNodePtr& node, std::string role);
    void nodeToHtml(const ProofNodePtr& node, std::string role);

    std::string getHtmlPrefix() const;
    std::string getHtmlSuffix() const;
};

} // namespace TptpTool

#include "ProofPrinter.hpp"

#include "../ExpressionUtils.hpp"
#include "TptpProofNode.hpp"

#include <algorithm>
#include <cassert>

namespace TptpTool {

ProofPrinter::ProofPrinter(Format format) :
    format(format), idCounter(0),
    exprPrinter(
        format == Format::TEXT_UTF8 ? ExpressionPrinter::Config::textUtf8() :
        format == Format::TSTP      ? ExpressionPrinter::Config::tptp() :
        ExpressionPrinter::Config::latex()) {
}

std::string ProofPrinter::toString(const ProofNodePtr& proofRoot) {
    if (!proofRoot) return "";

    nodeIds.clear();
    idCounter = 0;
    outputBuffer.str("");
    outputBuffer.clear();
    textOutputBuffer.clear();

    if (format == Format::HTML) outputBuffer << getHtmlPrefix();

    std::unordered_set<const ProofNode*> visited;
    processOnlyTptpNodes(proofRoot, visited);
    processNodeRecursively(proofRoot);

    if (format == Format::TEXT_UTF8) processTextOutputBuffer();
    if (format == Format::HTML) outputBuffer << getHtmlSuffix();

    return outputBuffer.str();
}

void ProofPrinter::processOnlyTptpNodes(const ProofNodePtr& node,
    std::unordered_set<const ProofNode*>& visited) {
    if (!node || visited.count(node.get())) return;
    visited.insert(node.get());
    if (auto tptpNode = std::dynamic_pointer_cast<TptpProofNode>(node)) {
        processNodeRecursively(node);
    }
    else if (auto stepNode = std::dynamic_pointer_cast<ProofStep>(node)) {
        for (const auto& parent : stepNode->getParents()) {
            processOnlyTptpNodes(parent, visited);
        }
    }
}

void ProofPrinter::processNodeRecursively(const ProofNodePtr& node) {
    if (!node || nodeIds.find(node.get()) != nodeIds.end()) return;
    if (auto stepNode = std::dynamic_pointer_cast<ProofStep>(node)) {
        for (const auto& parent : stepNode->getParents()) {
            processNodeRecursively(parent);
        }
    }

    std::string id = std::to_string(++idCounter);
    nodeIds[node.get()] = id;

    std::string role;
    if (auto tptpNode = std::dynamic_pointer_cast<TptpProofNode>(node)) {
        role = tptpNode->getRole();
    }
    else {
        switch (node->getType()) {
        case ProofNode::Type::CONJECTURE: role = "conjecture"; break;
        case ProofNode::Type::NEGATED_CONJECTURE: role = "negated_conjecture"; break;
        case ProofNode::Type::PREMISE: role = "axiom"; break;
        case ProofNode::Type::INFERENCE: default: role = "plain"; break;
        }
    }

    switch (format) {
    case Format::TEXT_UTF8: nodeToTextUtf8(node, role); break;
    case Format::TSTP:      nodeToTstp(node, role); break;
    case Format::HTML:      nodeToHtml(node, role); break;
    default: assert(false);
    }
}

void ProofPrinter::processTextOutputBuffer() {
    if (textOutputBuffer.empty()) return;

    auto getUtf8StrLength = [](const std::string& s) -> size_t {
        size_t length = 0;
        for (unsigned char c : s) if ((c & 0xC0) != 0x80) length++;
        return length;
    };

    const size_t MIN_HEAD_PADDING = 2;
    const size_t MIN_FORMULA_PADDING = 4;

    size_t maxHeadLength = 0;
    size_t maxFormulaLength = 0;
    size_t sumFormulaLength = 0;
    for (const auto& row : textOutputBuffer) {
        auto headLength = getUtf8StrLength(row[0]);
        auto formulaLength = getUtf8StrLength(row[1]);
        maxHeadLength = std::max(headLength, maxHeadLength);
        maxFormulaLength = std::max(formulaLength, maxFormulaLength);
        sumFormulaLength += formulaLength;
    }
    auto averageFormulaLength = sumFormulaLength / textOutputBuffer.size();

    size_t headWidth = maxHeadLength + MIN_HEAD_PADDING;
    auto suggestedFormulaWidth = static_cast<size_t>(averageFormulaLength * 2.5);
    size_t formulaMinWidth = std::min(suggestedFormulaWidth, maxFormulaLength);

    for (const auto& row : textOutputBuffer) {
        const std::string& head = row[0];
        const std::string& formula = row[1];
        const std::string& source = row[2];

        size_t headPadding = headWidth - getUtf8StrLength(head);
        outputBuffer << head << std::string(headPadding, ' ');

        size_t formulaLength = getUtf8StrLength(formula);
        size_t formulaPadding = MIN_FORMULA_PADDING;
        if (formulaLength < formulaMinWidth) {
            formulaPadding += (formulaMinWidth - formulaLength);
        }
        outputBuffer << formula << std::string(formulaPadding, ' ');

        outputBuffer << source << "\n";
    }
}

void ProofPrinter::nodeToTextUtf8(const ProofNodePtr& node, std::string role) {
    std::string headStr = "[" + nodeIds.at(node.get()) + "] " + role + ":";
    std::string formulaStr = exprPrinter.toString(node->getFormula());

    std::ostringstream sourceStream;
    sourceStream << "{ ";
    if (auto tptpNode = std::dynamic_pointer_cast<TptpProofNode>(node)) {
        std::string path = tptpNode->getSourceFile();
        std::replace(path.begin(), path.end(), '\\', '/');
        sourceStream << tptpNode->getName() << " | " << path;
    }
    else if (auto stepNode = std::dynamic_pointer_cast<ProofStep>(node)) {
        sourceStream << stepNode->getRule();
        const auto& parents = stepNode->getParents();
        assert(!parents.empty());
        sourceStream << " [";
        for (size_t i = 0; i < parents.size(); ++i) {
            sourceStream << nodeIds.at(parents[i].get());
            if (i < parents.size() - 1) sourceStream << ", ";
        }
        sourceStream << "]";
    }
    else {
        sourceStream << "unknown";
    }
    sourceStream << " }";

    textOutputBuffer.push_back({ std::move(headStr), std::move(formulaStr), sourceStream.str() });
}

void ProofPrinter::nodeToTstp(const ProofNodePtr& node, std::string role) {
    const bool isCnf = ExpressionUtils::isCnf(node->getFormula());

    outputBuffer << (isCnf ? "cnf" : "fof") << "("
        << nodeIds.at(node.get()) << ", "
        << role << ", "
        << exprPrinter.toString(node->getFormula()) << ", ";

    if (auto tptpNode = std::dynamic_pointer_cast<TptpProofNode>(node)) {
        std::string path = tptpNode->getSourceFile();
        std::replace(path.begin(), path.end(), '\\', '/');
        outputBuffer << "file('" << path << "', " << tptpNode->getName() << ")";
    }
    else if (auto stepNode = std::dynamic_pointer_cast<ProofStep>(node)) {
        outputBuffer << "inference(" << stepNode->getRule() << ", [], [";
        const auto& parents = stepNode->getParents();
        assert(!parents.empty());
        for (size_t i = 0; i < parents.size(); ++i) {
            outputBuffer << nodeIds.at(parents[i].get());
            if (i < parents.size() - 1) outputBuffer << ", ";
        }
        outputBuffer << "])";
    }
    else {
        outputBuffer << "unknown";
    }

    outputBuffer << ").\n";
}

void ProofPrinter::nodeToHtml(const ProofNodePtr& node, std::string role) {
    auto escapeHtml = [](const std::string& data) -> std::string {
        std::string buffer;
        buffer.reserve(data.size() * 2);
        for (char c : data) {
            switch (c) {
            case '&':  buffer += "&amp;"; break;
            case '\"': buffer += "&quot;"; break;
            case '\'': buffer += "&apos;"; break;
            case '<':  buffer += "&lt;"; break;
            case '>':  buffer += "&gt;"; break;
            default:   buffer += c; break;
            }
        }
        return buffer;
        };

    std::string rawId = nodeIds.at(node.get());
    std::string domId = "n" + rawId;

    std::string formulaStr = escapeHtml(exprPrinter.toString(node->getFormula()));

    std::stringstream parentIdsStream;
    if (auto stepNode = std::dynamic_pointer_cast<ProofStep>(node)) {
        for (const auto& parent : stepNode->getParents()) {
            if (nodeIds.count(parent.get())) {
                parentIdsStream << "n" << nodeIds.at(parent.get()) << " ";
            }
        }
    }
    std::string parentsAttr = parentIdsStream.str();
    if (!parentsAttr.empty()) parentsAttr.pop_back();

    outputBuffer << "<div class='proof-row' id='" << domId << "' data-parents='" << parentsAttr << "'>\n";
    outputBuffer << "  <div class='col-id'><a href='#" << domId << "'>" << rawId << "</a></div>\n";
    outputBuffer << "  <div class='col-role'><span class='role-badge role-" << role << "'>" << role << "</span></div>\n";
    outputBuffer << "  <div class='col-formula'>\\( " << formulaStr << " \\)</div>\n";
    outputBuffer << "  <div class='col-source'>";

    if (auto tptpNode = std::dynamic_pointer_cast<TptpProofNode>(node)) {
        outputBuffer << "<strong>" << escapeHtml(tptpNode->getName()) << "</strong>";
        outputBuffer << "&nbsp;&nbsp;|&nbsp;&nbsp;";
        std::string path = tptpNode->getSourceFile();
        std::replace(path.begin(), path.end(), '\\', '/');
        outputBuffer << "<span class='src-file'>" << escapeHtml(path) << "</span>";
    }
    else if (auto stepNode = std::dynamic_pointer_cast<ProofStep>(node)) {
        outputBuffer << "<strong>" << stepNode->getRule() << "</strong>";
        const auto& parents = stepNode->getParents();
        if (!parents.empty()) {
            outputBuffer << " <span class='src-parents'>[ ";
            for (size_t i = 0; i < parents.size(); ++i) {
                if (nodeIds.count(parents[i].get())) {
                    std::string pid = nodeIds.at(parents[i].get());
                    outputBuffer << "<a href='#n" << pid << "'>" << pid << "</a>";
                }
                else {
                    outputBuffer << "?";
                }
                if (i < parents.size() - 1) outputBuffer << ", ";
            }
            outputBuffer << " ]</span>";
        }
    }
    outputBuffer << "</div>\n";
    outputBuffer << "</div>\n";
}

std::string ProofPrinter::getHtmlPrefix() const {
    return R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Proof Visualization</title>
    <script>
        MathJax = { tex: { inlineMath: [['$', '$'], ['\\(', '\\)']] }, svg: { fontCache: 'global' } };
    </script>
    <script id="MathJax-script" async src="https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js"></script>
    <style>
        :root {
            --border: 1px solid #dee2e6;
            --bg-hover: #f8f9fa;
            --bg-highlight: #fff3cd;
            --clr-text: #212529;
            --clr-meta: #6c757d;
            --clr-link: #0d6efd;
            --font-main: system-ui, -apple-system, "Segoe UI", Roboto, "Helvetica Neue", Arial;
            --font-mono: SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
        }

        body {
            font-family: var(--font-main);
            color: var(--clr-text);
            margin: 0;
            padding: 20px;
            background-color: #fff;
        }

        .proof-container {
            border: var(--border);
            border-radius: 6px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.05);
            width: fit-content;
            max-width: 98%;
            margin: 0 auto;
            overflow: hidden; 
            display: flex;
            flex-direction: column;
        }

        .proof-title {
            padding: 12px 15px;
            background-color: #f8f9fa;
            border-bottom: var(--border);
            font-size: 1.25rem;
            font-weight: 500;
        }

        .proof-body-wrapper {
            overflow-x: auto;
            width: 100%;
        }

        .proof-body {
            display: grid;
            grid-template-columns: max-content max-content minmax(300px, 1fr) max-content;
        }

        .proof-row { display: contents; }

        .proof-row > div {
            padding: 8px 12px;
            border-bottom: 1px solid #f1f1f1;
            display: flex;
            align-items: center;
        }

        .row-hover > div { background-color: var(--bg-hover); }
        .row-highlight > div { background-color: var(--bg-highlight); transition: background 0.2s; }

        .col-id {
            font-family: var(--font-mono);
            font-weight: bold;
            color: var(--clr-meta);
            border-right: 1px solid #eee;
            justify-content: flex-end;
            white-space: nowrap;
        }

        .col-role { justify-content: center; padding-left: 15px; padding-right: 15px; }

        .col-formula {
            font-family: 'Latin Modern Math', 'Cambria Math', serif;
            font-size: 1.15em;
            overflow-x: auto;
            white-space: nowrap;
        }
        .col-formula::-webkit-scrollbar { height: 4px; }
        .col-formula::-webkit-scrollbar-thumb { background: #ccc; }

        .col-source {
            border-left: 1px solid #eee;
            font-size: 0.9em;
            color: var(--clr-meta);
            white-space: nowrap;
        }

        a { text-decoration: none; color: inherit; }
        .col-source a { color: var(--clr-link); }
        .col-id a { color: inherit; text-decoration: none; cursor: pointer; }
        .col-id a:hover { text-decoration: underline; color: #000; }
        .col-source a:hover { text-decoration: underline; }

        .src-file { color: #495057; }
        .src-parents { color: #888; margin-left: 5px; }

        /* --- ROLE STYLES --- */
        .role-text {
            font-size: 0.75rem;
            font-weight: 600;
            letter-spacing: 0.5px;
            text-transform: lowercase;
            font-variant: small-caps;
        }
        
        /* Group 1: Base Truths & Definitions (Dark Green) */
        .role-axiom, .role-hypothesis, 
        .role-definition, .role-assumption, 
        .role-lemma, .role-theorem { 
            color: #198754; 
        }
        
        /* Group 2: Conflicts & Goals (Red/Orange) */
        .role-negated_conjecture { color: #d32f2f; }
        .role-conjecture { color: #e65100; }
        
        /* Group 3: Steps & Technical (Subtle Grey) */
        .role-plain, .role-unknown, 
        .role-type, .role-fi_domain, .role-fi_functors, .role-fi_predicates { 
            color: #adb5bd; 
            font-weight: 500; 
        }
        
        @media (max-width: 800px) {
             .proof-body { grid-template-columns: 1fr; }
             .proof-row { display: flex; flex-direction: column; border-bottom: 4px solid #eee; }
             .proof-row > div { border: none; width: 100%; justify-content: flex-start; padding: 4px 10px; }
             .col-id { border-right: none; }
             .col-source { border-left: none; }
             .col-formula { white-space: normal; overflow-x: visible; }
        }
    </style>
</head>
<body>
    <div class="proof-container">
        <div class="proof-title">Proof</div>
        <div class="proof-body-wrapper">
            <div class="proof-body">
)";
}

std::string ProofPrinter::getHtmlSuffix() const {
    return R"(
            </div>
        </div>
    </div>

<script>
    document.addEventListener('DOMContentLoaded', () => {
        // Scroll handling for display: contents
        document.querySelectorAll('a[href^="#"]').forEach(anchor => {
            anchor.addEventListener('click', function (e) {
                e.preventDefault();
                const targetId = this.getAttribute('href').substring(1);
                const targetRow = document.getElementById(targetId);
                
                if (targetRow) {
                    const firstChild = targetRow.firstElementChild;
                    if (firstChild) {
                        firstChild.scrollIntoView({ behavior: 'smooth', block: 'center' });
                        highlightRow(targetId, 'row-hover');
                        setTimeout(() => unhighlightRow(targetId, 'row-hover'), 1500);
                    }
                }
            });
        });

        const rows = document.querySelectorAll('.proof-row');

        rows.forEach(row => {
            row.addEventListener('mouseenter', () => {
                const id = row.id;
                highlightRow(id, 'row-hover');
                const parents = row.getAttribute('data-parents');
                if (parents) {
                    parents.split(' ').forEach(pid => highlightRow(pid, 'row-highlight'));
                }
            });

            row.addEventListener('mouseleave', () => {
                const id = row.id;
                unhighlightRow(id, 'row-hover');
                const parents = row.getAttribute('data-parents');
                if (parents) {
                    parents.split(' ').forEach(pid => unhighlightRow(pid, 'row-highlight'));
                }
            });
        });

        function highlightRow(id, className) {
            if (!id) return;
            const row = document.getElementById(id);
            if (row) row.classList.add(className);
        }

        function unhighlightRow(id, className) {
            if (!id) return;
            const row = document.getElementById(id);
            if (row) row.classList.remove(className);
        }
    });
</script>
</body>
</html>
)";
}

} // namespace TptpTool

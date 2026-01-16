#pragma once

#include "../Expression.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace TptpTool {

class Loader {
public:
    struct AnnotatedFormula {
        std::string type;
        std::string name;
        std::string role;
        FormulaPtr formula;
        std::string annotations;
        std::string sourceFile;
    };

    struct IncludeDirective {
        std::string filePath;
        std::vector<std::string> filter;
    };

    struct FileParseResult {
        std::vector<AnnotatedFormula> formulas;
        std::vector<IncludeDirective> includes;
    };

    explicit Loader(std::string tptpDir = std::string());

    FileParseResult loadFromText(const std::string& text);
    FileParseResult loadSingleFile(const std::string& filePath);
    std::vector<AnnotatedFormula> loadRecursively(const std::string& rootFilePath);

private:
    const std::string tptpDir;

    void loadRecursivelyImpl(
        const std::string& resolvedPath,
        const std::string& relativePath,
        std::vector<AnnotatedFormula>& accumulator,
        std::unordered_set<std::string>& visitedPaths,
        std::unordered_set<std::string>& loadedFormulaNames,
        std::unordered_map<std::string, FileParseResult>& parsedFileCache);

    std::string resolvePath(
        const std::string& relativePath,
        const std::string& tptpDirPath,
        const std::string& contextFilePath = "");
};

} // namespace TptpTool

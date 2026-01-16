#include "Lexer.hpp"
#include "Loader.hpp"
#include "Parser.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace TptpTool {

Loader::Loader(std::string tptpDir)
    : tptpDir(std::move(tptpDir)) {}

Loader::FileParseResult Loader::loadFromText(const std::string& text) {
    Lexer lexer(text);
    std::vector<Token> tokens = lexer.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

Loader::FileParseResult Loader::loadSingleFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file) throw std::runtime_error("Cannot open file: " + filePath);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return loadFromText(content);
}

std::vector<Loader::AnnotatedFormula> Loader::loadRecursively(const std::string& rootFilePath) {
    std::vector<AnnotatedFormula> result;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> loadedFormulaNames;
    std::unordered_map<std::string, FileParseResult> parsedFileCache;
    loadRecursivelyImpl(resolvePath(rootFilePath, tptpDir), rootFilePath, result,
        visited, loadedFormulaNames, parsedFileCache);
    return result;
}

void Loader::loadRecursivelyImpl(
    const std::string& resolvedPath,
    const std::string& relativePath,
    std::vector<AnnotatedFormula>& accumulator,
    std::unordered_set<std::string>& visited,
    std::unordered_set<std::string>& loadedFormulaNames,
    std::unordered_map<std::string, FileParseResult>& parsedFileCache) {

    std::string canonicalPath;
    try {
        canonicalPath = std::filesystem::canonical(resolvedPath).string();
    }
    catch (...) {
        throw std::runtime_error("Invalid file path: " + resolvedPath);
    }

    if (visited.count(canonicalPath)) return;
    visited.insert(canonicalPath);

    auto cacheIt = parsedFileCache.find(canonicalPath);
    if (cacheIt == parsedFileCache.end()) {
        cacheIt = parsedFileCache.emplace(canonicalPath, loadSingleFile(canonicalPath)).first;
    }
    const FileParseResult& parsed = cacheIt->second;

    for (const auto& include : parsed.includes) {
        std::string nextResolvedPath = resolvePath(include.filePath, tptpDir, canonicalPath);
        if (include.filter.empty()) {
            loadRecursivelyImpl(nextResolvedPath, include.filePath, accumulator,
                visited, loadedFormulaNames, parsedFileCache);
        }
        else {
            std::unordered_set<std::string> tempLoadedFormulaNames = loadedFormulaNames;
            std::vector<AnnotatedFormula> importedFormulas;
            loadRecursivelyImpl(nextResolvedPath, include.filePath, importedFormulas,
                visited, tempLoadedFormulaNames, parsedFileCache);
            std::unordered_set<std::string> selection(
                include.filter.begin(), include.filter.end());
            for (auto& formula : importedFormulas) {
                if (selection.count(formula.name)) {
                    if (loadedFormulaNames.insert(formula.name).second) {
                        accumulator.push_back(std::move(formula));
                    }
                }
            }
        }
    }

    for (auto& formula : parsed.formulas) {
        if (loadedFormulaNames.insert(formula.name).second) {
            AnnotatedFormula copy = formula;
            copy.sourceFile = relativePath;
            accumulator.push_back(std::move(copy));
        }
    }

    visited.erase(canonicalPath);
}

std::string Loader::resolvePath(
    const std::string& relativePath,
    const std::string& tptpDirPath,
    const std::string& contextFilePath) {
    namespace fs = std::filesystem;

    fs::path relative(relativePath);
    fs::path tptpDir(tptpDirPath);
    fs::path contextFile(contextFilePath);

    if (relative.is_absolute() && fs::exists(relative)) {
        return relative.string();
    }

    if (!contextFilePath.empty()) {
        auto path = contextFile.parent_path() / relative;
        if (fs::exists(path)) return fs::absolute(path).string();
    }

    if (!tptpDirPath.empty()) {
        auto path = tptpDir / relative;
        if (fs::exists(path)) return fs::absolute(path).string();
    }

    if (fs::exists(relative)) {
        return fs::absolute(relative).string();
    }

    throw std::runtime_error("File not found: " + relativePath);
}

} // namespace TptpTool

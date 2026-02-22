#include "Solver.hpp"

#include "../Expression.hpp"
#include "../ExpressionTransformer.hpp"
#include "../NaiveResolutionSolver.hpp"
#include "../NaiveSuperpositionSolver.hpp"
#include "../ProofNode.hpp"
#include "../SuperpositionSolver.hpp"
#include "ProofPrinter.hpp"
#include "TPTPProofNode.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

namespace TptpTool {

Solver::Solver(std::string inFilePath, std::string tptpDir, bool prepareProof)
    : inFilePath(std::move(inFilePath)), tptpDir(std::move(tptpDir)), prepareProof(prepareProof) {
}

std::string Solver::getTextProof() const {
    return textProof;
}

std::string Solver::getTstpProof() const {
    return tstpProof;
}

std::string Solver::getHtmlProof() const {
    return htmlProof;
}

Solver::OutStatus Solver::solve(int timeLimitSeconds, int memoryLimitMegabytes,
                                const std::string& solverName,
                                const std::string& answerPredicate) {
    std::vector<Loader::AnnotatedFormula> annotatedFormulas;
    Loader loader(tptpDir);
    try {
        annotatedFormulas = loader.loadRecursively(inFilePath);
    }
    catch (const std::exception& e) {
        std::cerr << "Loader Error: " << e.what() << std::endl;
        return OutStatus::INPUT_ERROR;
    }

    auto problemDef = createProblemDef(annotatedFormulas);
    auto clauseNodes = convertToCnf(problemDef.formulaNodes);

    std::unique_ptr<FolSatSolver> cnfSolver;
    if (solverName.empty()) {
        cnfSolver = std::make_unique<SuperpositionSolver>();
    }
    else if (solverName == "naive_resolution") {
        cnfSolver = std::make_unique<NaiveResolutionSolver>();
    }
    else if (solverName == "naive_superposition") {
        cnfSolver = std::make_unique<NaiveSuperpositionSolver>();
    }
    else if (solverName == "superposition") {
        cnfSolver = std::make_unique<SuperpositionSolver>();
    }
    else {
        std::cerr << "Unsupported solver: " << solverName << std::endl;
        return OutStatus::INPUT_ERROR;
    }

    if (timeLimitSeconds > 0) cnfSolver->setTimeLimit(timeLimitSeconds);
    if (memoryLimitMegabytes > 0) cnfSolver->setMemoryLimit(memoryLimitMegabytes);
    if (!answerPredicate.empty()) cnfSolver->setAnswerPredicateSymbol(answerPredicate);
    FolSatSolver::Result result = cnfSolver->solve(clauseNodes);

    Solver::OutStatus outStatus;

    switch (result) {
    case FolSatSolver::Result::UNSATISFIABLE:
        outStatus = problemDef.isRefutation ?
            OutStatus::THEOREM : OutStatus::UNSATISFIABLE;
        break;
    case FolSatSolver::Result::SATISFIABLE:
        outStatus = problemDef.isRefutation ?
            OutStatus::COUNTER_SATISFIABLE : OutStatus::SATISFIABLE;
        break;
    case FolSatSolver::Result::TIME_OUT:
        return OutStatus::TIME_OUT;
    case FolSatSolver::Result::MEMORY_OUT:
        return OutStatus::MEMORY_OUT;
    default:
        return OutStatus::UNKNOWN;
    };

    if (prepareProof) {
        ProofNodePtr proofRoot = cnfSolver->getProof();
        if (proofRoot) {
            ProofPrinter textPrinter(ProofPrinter::Format::TEXT_UTF8);
            this->textProof = textPrinter.toString(proofRoot);
            ProofPrinter tstpPrinter(ProofPrinter::Format::TSTP);
            this->tstpProof = tstpPrinter.toString(proofRoot);
            ProofPrinter htmlPrinter(ProofPrinter::Format::HTML);
            this->htmlProof = htmlPrinter.toString(proofRoot);
        }
    }

    return outStatus;
}

Solver::ProblemDef Solver::createProblemDef(
    std::vector<Loader::AnnotatedFormula> annotatedFormulas) const {
    ProblemDef problemDef;
    problemDef.isRefutation = false;

    std::vector<ProofNodePtr> conjectureNodes;

    for (auto& af : annotatedFormulas) {
        bool isConjecture = af.role == "conjecture";
        bool isNegConjecture = af.role == "negated_conjecture";

        if (isConjecture) {
            problemDef.isRefutation = true;
        }
        ProofNode::Type nodeType = ProofNode::Type::PREMISE;
        if (isConjecture) nodeType = ProofNode::Type::CONJECTURE;
        if (isNegConjecture) nodeType = ProofNode::Type::NEGATED_CONJECTURE;

        auto tptpNode = TptpProofNode::create(
            af.formula,
            nodeType,
            af.name,
            af.sourceFile,
            af.role
        );
        if (isConjecture) conjectureNodes.push_back(tptpNode);
        else problemDef.formulaNodes.push_back(tptpNode);
    }

    if (!conjectureNodes.empty()) {
        FormulaPtr combinedConjecture;

        if (conjectureNodes.size() == 1) {
            combinedConjecture = conjectureNodes.front()->getFormula();
        }
        else {
            std::vector<FormulaPtr> conjectureFormulas;
            conjectureFormulas.reserve(conjectureNodes.size());
            for (const auto& node : conjectureNodes) {
                conjectureFormulas.push_back(node->getFormula());
            }
            combinedConjecture = std::make_shared<JunctionFormula>(
                JunctionFormula::Operator::AND, std::move(conjectureFormulas));
        }

        auto negatedConjecture = std::make_shared<NegationFormula>(combinedConjecture);
        auto negConjectureNode = ProofStep::create(
            negatedConjecture,
            ProofNode::Type::NEGATED_CONJECTURE,
            "negate_conjecture",
            conjectureNodes
        );

        problemDef.formulaNodes.push_back(negConjectureNode);
    }
    return problemDef;
}

std::vector<ProofNodePtr> Solver::convertToCnf(const std::vector<ProofNodePtr>& nodes) const {
    std::vector<ProofNodePtr> clauseNodes;
    ExpressionTransformer et;

    for (const auto& node : nodes) {
        et.getNameRegistry()->registerPredAndFuncNames(node->getFormula());
    }

    for (const auto& node : nodes) {
        auto clauses = et.toCnf(node->getFormula());
        for (const auto& clause : clauses) {
            auto clauseNode = ProofStep::create(
                clause,
                ProofNode::Type::INFERENCE,
                "cnf_transformation",
                { node }
            );
            clauseNodes.push_back(clauseNode);
        }
    }
    return clauseNodes;
}

} // namespace TptpTool

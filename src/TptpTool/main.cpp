#define _CRT_SECURE_NO_WARNINGS // std::getenv()

#include "Solver.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace TptpTool;

struct Options {
    std::string filePath;
    int timeLimitSeconds = -1;
    int memoryLimit = -1;
    bool printProof = false;
    bool printHelp = false;
    std::string solverName;
    std::string ansPredicate;
};

Options parseArguments(int argc, char* argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            options.printHelp = true;
        }
        else if (arg == "-p" || arg == "--proof") {
            options.printProof = true;
        }
        else if (arg == "-t" || arg == "--time-limit") {
            if (i + 1 < argc) {
                try {
                    options.timeLimitSeconds = std::stoi(argv[++i]);
                }
                catch (...) {
                    throw std::runtime_error("Invalid integer format for time limit: " + std::string(argv[i]));
                }
            }
            else {
                throw std::runtime_error("Missing argument for time limit option");
            }
        }
        else if (arg == "-m" || arg == "--memory-limit") {
            if (i + 1 < argc) {
                try {
                    options.memoryLimit = std::stoi(argv[++i]);
                }
                catch (...) {
                    throw std::runtime_error("Invalid integer format for memory limit: " + std::string(argv[i]));
                }
            }
            else {
                throw std::runtime_error("Missing argument for memory limit option");
            }
        }
        else if (arg == "-s" || arg == "--solver") {
            if (i + 1 < argc) {
                options.solverName = argv[++i];
            }
            else {
                throw std::runtime_error("Missing argument for solver option");
            }
        }
        else if (arg == "-a" || arg == "--answer-predicate") {
            if (i + 1 < argc) {
                options.ansPredicate = argv[++i];
            }
            else {
                throw std::runtime_error("Missing argument for answer predicate option");
            }
        }
        else if (arg[0] == '-') {
            throw std::runtime_error("Unknown option: " + arg);
        }
        else {
            if (!options.filePath.empty()) {
                throw std::runtime_error("Multiple input files provided");
            }
            options.filePath = arg;
        }
    }

    if (options.filePath.empty() && !options.printHelp) {
        throw std::runtime_error("No input file specified");
    }

    return options;
}

std::string extractFileName(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
}

std::string getUsageString(std::string programName) {
    std::stringstream ss;
    ss << "Usage: " << programName << " [options] <input_file>\n"
        << "Options:\n"
        << "  -t, --time-limit <sec>           CPU time limit in seconds\n"
        << "  -m, --memory-limit <MB>          Memory limit in megabytes\n"
        << "  -p, --proof                      Output proof if found\n"
        << "  -s, --solver <solver_name>       Choose solver\n"
        << "  -a, --answer-predicate <symbol>  Set answer (mode) predicate\n"
        << "  -h, --help                       Show this help message\n";
    return ss.str();
}

inline std::string statusToString(Solver::OutStatus status) {
    switch (status) {
    case Solver::OutStatus::THEOREM:              return "Theorem";
    case Solver::OutStatus::COUNTER_SATISFIABLE:  return "CounterSatisfiable";
    case Solver::OutStatus::UNSATISFIABLE:        return "Unsatisfiable";
    case Solver::OutStatus::SATISFIABLE:          return "Satisfiable";
    case Solver::OutStatus::TIME_OUT:             return "Timeout";
    case Solver::OutStatus::MEMORY_OUT:           return "MemoryOut";
    case Solver::OutStatus::INPUT_ERROR:          return "InputError";
    case Solver::OutStatus::OS_ERROR:             return "OSError";
    case Solver::OutStatus::ERROR:                return "Error";
    default:                                      return "Unknown";
    }
}

inline void printStatus(Solver::OutStatus status, const std::string& fileName = "") {
    std::cout << "% SZS status " << statusToString(status);
    if (!fileName.empty()) {
        auto path = fileName;
        std::replace(path.begin(), path.end(), '\\', '/');
        std::cout << " for " << path;
    }
    std::cout << std::endl;
}

inline void printProof(const std::string& proof) {
    std::cout << "% SZS output start Proof" << std::endl;
    std::cout << proof;
    if (proof.back() != '\n') std::cout << std::endl;
    std::cout << "% SZS output end Proof" << std::endl;    
}

inline void saveProof(const std::string& proof, std::string fileName) {
    std::ofstream outFile(fileName);
    if (outFile.is_open()) {
        outFile << proof;
    }
    else {
        throw std::runtime_error("Cannot open file: " + fileName);
    }
}

int main(int argc, char* argv[]) {
    const std::string programName = extractFileName(argv[0]);
    const char* envName = "TPTP_DIR";
    const char* envVal = std::getenv(envName);
    std::string tptpDir = envVal ? envVal : "";

    Options options;

    try {
        try {
            options = parseArguments(argc, argv);
        }
        catch (const std::exception& e) {
            printStatus(Solver::OutStatus::INPUT_ERROR);
            std::cerr << "Error: " << e.what() << std::endl;
            std::cerr << "Try '" << programName << " --help' for more information" << std::endl;
            return 1;
        }

        if (options.printHelp) {
            std::cout << getUsageString(programName);
            return 0;
        }

        Solver solver(options.filePath, tptpDir, options.printProof);
        Solver::OutStatus status = solver.solve(options.timeLimitSeconds, options.memoryLimit,
                                                options.solverName, options.ansPredicate);
        printStatus(status, options.filePath);
        if (options.printProof) {
            auto textProof = solver.getTextProof();
            if (!textProof.empty()) {
                saveProof(textProof, "lastProof.txt");
            }
            auto tstpProof = solver.getTstpProof();
            if (!tstpProof.empty()) {
                printProof(tstpProof);
            }
            auto htmlProof = solver.getHtmlProof();
            if (!htmlProof.empty()) {
                saveProof(htmlProof, "lastProof.html");
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        printStatus(Solver::OutStatus::ERROR, options.filePath);
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown critical error" << std::endl;
        printStatus(Solver::OutStatus::ERROR, options.filePath);
        return 1;
    }

    return 0;
}

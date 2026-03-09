#include "../include/LevelLoader.h"
#include "../include/Solver.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// 获取用户输入，如果直接回车则返回默认值
template <typename T>
T getUserInput(const std::string& prompt, T defaultValue) {
    std::cout << prompt << " [Default: " << defaultValue << "]: ";
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return defaultValue;

    std::stringstream ss(line);
    T val;
    if (ss >> val) return val;
    return defaultValue;
}

bool parseBool01(const std::string& s, bool fallback) {
    if (s == "0") return false;
    if (s == "1") return true;
    return fallback;
}

std::string getStrategyName(SolveStrategy strategy) {
    if (strategy == SolveStrategy::AnytimeWeightedAStar) return "Anytime Weighted A*";
    if (strategy == SolveStrategy::PortfolioSearch) return "Portfolio Search";
    if (strategy == SolveStrategy::GreedyBestFirst) return "Greedy Best-First";
    if (strategy == SolveStrategy::ARAStar) return "ARA*";
    if (strategy == SolveStrategy::MHAStar) return "MHA*";
    return "Weighted A*";
}

bool strategyUsesWeight(SolveStrategy strategy) {
    return strategy == SolveStrategy::WeightedAStar ||
           strategy == SolveStrategy::AnytimeWeightedAStar ||
           strategy == SolveStrategy::PortfolioSearch ||
           strategy == SolveStrategy::ARAStar ||
           strategy == SolveStrategy::MHAStar;
}

bool isDifficultEggLevel(size_t oneBasedIndex) {
    return oneBasedIndex == 5 || oneBasedIndex == 6 || oneBasedIndex == 7 ||
           (oneBasedIndex >= 16 && oneBasedIndex <= 20);
}

void applyCliOverrides(int argc, char** argv, SolverConfig& config, bool& difficultOnly) {
    auto readValue = [&](int& i, const std::string& key) -> std::string {
        std::string arg = argv[i];
        std::string withEq = key + "=";
        if (arg.rfind(withEq, 0) == 0) return arg.substr(withEq.size());
        if (arg == key && i + 1 < argc) return argv[++i];
        return "";
    };

    for (int i = 1; i < argc; ++i) {
        std::string strategy = readValue(i, "--strategy");
        if (!strategy.empty()) {
            if (strategy == "1") config.strategy = SolveStrategy::AnytimeWeightedAStar;
            else if (strategy == "2") config.strategy = SolveStrategy::PortfolioSearch;
            else if (strategy == "3") config.strategy = SolveStrategy::GreedyBestFirst;
            else if (strategy == "4") config.strategy = SolveStrategy::ARAStar;
            else if (strategy == "5") config.strategy = SolveStrategy::MHAStar;
            else config.strategy = SolveStrategy::WeightedAStar;
            continue;
        }

        std::string difficultMode = readValue(i, "--difficult-only");
        if (!difficultMode.empty()) {
            difficultOnly = parseBool01(difficultMode, difficultOnly);
            continue;
        }

        std::string weight = readValue(i, "--weight");
        if (!weight.empty()) {
            std::stringstream ss(weight);
            double val;
            if (ss >> val) config.weight = val;
            continue;
        }

        std::string maxNodes = readValue(i, "--max-nodes");
        if (!maxNodes.empty()) {
            std::stringstream ss(maxNodes);
            int val;
            if (ss >> val) config.maxNodes = val;
            continue;
        }

        std::string pruning = readValue(i, "--pruning");
        if (!pruning.empty()) {
            config.usePruning = parseBool01(pruning, config.usePruning);
            continue;
        }

        std::string deadlockLevel = readValue(i, "--deadlock-level");
        if (!deadlockLevel.empty()) {
            std::stringstream ss(deadlockLevel);
            int val;
            if (ss >> val) config.deadlockLevel = (val >= 2 ? 2 : 1);
            continue;
        }

        std::string minWeight = readValue(i, "--anytime-min-weight");
        if (!minWeight.empty()) {
            std::stringstream ss(minWeight);
            double val;
            if (ss >> val) config.anytimeMinWeight = val;
            continue;
        }

        std::string decay = readValue(i, "--anytime-decay");
        if (!decay.empty()) {
            std::stringstream ss(decay);
            double val;
            if (ss >> val) config.anytimeWeightDecay = val;
            continue;
        }

        std::string passCount = readValue(i, "--portfolio-pass-count");
        if (!passCount.empty()) {
            std::stringstream ss(passCount);
            int val;
            if (ss >> val) config.portfolioPassCount = val;
            continue;
        }

        std::string togglePruning = readValue(i, "--portfolio-toggle-pruning");
        if (!togglePruning.empty()) {
            config.portfolioTogglePruning = parseBool01(togglePruning, config.portfolioTogglePruning);
            continue;
        }

        std::string araMin = readValue(i, "--ara-min-epsilon");
        if (!araMin.empty()) {
            std::stringstream ss(araMin);
            double val;
            if (ss >> val) config.araMinEpsilon = val;
            continue;
        }

        std::string araDecay = readValue(i, "--ara-decay");
        if (!araDecay.empty()) {
            std::stringstream ss(araDecay);
            double val;
            if (ss >> val) config.araDecay = val;
            continue;
        }

        std::string mhaSecondary = readValue(i, "--mha-secondary-weight");
        if (!mhaSecondary.empty()) {
            std::stringstream ss(mhaSecondary);
            double val;
            if (ss >> val) config.mhaSecondaryWeight = val;
            continue;
        }

        std::string mhaBias = readValue(i, "--mha-anchor-bias");
        if (!mhaBias.empty()) {
            std::stringstream ss(mhaBias);
            double val;
            if (ss >> val) config.mhaAnchorBias = val;
            continue;
        }
    }
}

void printLevelList(const std::string& label, const std::vector<int>& levels) {
    if (levels.empty()) {
        std::cout << label << ": none" << std::endl;
        return;
    }
    std::cout << label << " (" << levels.size() << "): ";
    for (size_t i = 0; i < levels.size(); ++i) {
        if (i) std::cout << ", ";
        std::cout << levels[i];
    }
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    // --- 1. 用户配置 ---
    std::cout << "=== Bobby's Solver Configuration ===" << std::endl;

    SolverConfig config;
    bool difficultOnly = true;
    applyCliOverrides(argc, argv, config, difficultOnly);

    std::cout << "Only solve difficult egg subset? (Default Yes: egg 5,6,7,16-20) (0=No, 1=Yes) [Default: "
              << (difficultOnly ? 1 : 0) << "]: ";
    std::string difficultLine;
    std::getline(std::cin, difficultLine);
    if (!difficultLine.empty()) {
        difficultOnly = difficultLine != "0";
    }

    int strategyDefault = 0;
    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) strategyDefault = 1;
    else if (config.strategy == SolveStrategy::PortfolioSearch) strategyDefault = 2;
    else if (config.strategy == SolveStrategy::GreedyBestFirst) strategyDefault = 3;
    else if (config.strategy == SolveStrategy::ARAStar) strategyDefault = 4;
    else if (config.strategy == SolveStrategy::MHAStar) strategyDefault = 5;

    std::cout << "Solve Strategy (0=Weighted A*, 1=Anytime Weighted A*, 2=Portfolio Search, 3=Greedy Best-First, 4=ARA*, 5=MHA*) [Default: " << strategyDefault << "]: ";
    std::string strategyLine;
    std::getline(std::cin, strategyLine);
    if (!strategyLine.empty()) {
        if (strategyLine == "1") config.strategy = SolveStrategy::AnytimeWeightedAStar;
        else if (strategyLine == "2") config.strategy = SolveStrategy::PortfolioSearch;
        else if (strategyLine == "3") config.strategy = SolveStrategy::GreedyBestFirst;
        else if (strategyLine == "4") config.strategy = SolveStrategy::ARAStar;
        else if (strategyLine == "5") config.strategy = SolveStrategy::MHAStar;
        else config.strategy = SolveStrategy::WeightedAStar;
    }

    if (strategyUsesWeight(config.strategy)) {
        config.weight = getUserInput<double>("Search Weight (1.0 = optimal, >1.0 = faster)", config.weight);
    }
    config.maxNodes = getUserInput<int>("Max Nodes Limit", config.maxNodes);

    std::cout << "Use Reachability Pruning? (0=No, 1=Yes) [Default: " << (config.usePruning ? 1 : 0) << "]: ";
    std::string line;
    std::getline(std::cin, line);
    if (!line.empty()) {
        config.usePruning = line != "0";
    }
    if (config.usePruning) {
        config.deadlockLevel = getUserInput<int>("Deadlock Level (1=Basic reachability, 2=Strict with key/end checks)", config.deadlockLevel);
        if (config.deadlockLevel < 1) config.deadlockLevel = 1;
        if (config.deadlockLevel > 2) config.deadlockLevel = 2;
    }

    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) {
        config.anytimeMinWeight = getUserInput<double>("Anytime min weight (>=1.0)", config.anytimeMinWeight);
        config.anytimeWeightDecay = getUserInput<double>("Anytime weight decay (0~1, e.g. 0.5)", config.anytimeWeightDecay);
    } else if (config.strategy == SolveStrategy::PortfolioSearch) {
        config.portfolioPassCount = getUserInput<int>("Portfolio pass count", config.portfolioPassCount);
        std::cout << "Portfolio toggle pruning between passes? (0=No, 1=Yes) [Default: " << (config.portfolioTogglePruning ? 1 : 0) << "]: ";
        std::string portfolioLine;
        std::getline(std::cin, portfolioLine);
        if (!portfolioLine.empty()) {
            config.portfolioTogglePruning = portfolioLine != "0";
        }
    } else if (config.strategy == SolveStrategy::ARAStar) {
        config.araMinEpsilon = getUserInput<double>("ARA* min epsilon (>=1.0)", config.araMinEpsilon);
        config.araDecay = getUserInput<double>("ARA* epsilon decay (0~1, e.g. 0.8)", config.araDecay);
    } else if (config.strategy == SolveStrategy::MHAStar) {
        config.mhaSecondaryWeight = getUserInput<double>("MHA* secondary heuristic weight", config.mhaSecondaryWeight);
        config.mhaAnchorBias = getUserInput<double>("MHA* anchor bias", config.mhaAnchorBias);
    }

    std::cout << "------------------------------------" << std::endl;
    std::cout << "Config: DifficultOnly=" << (difficultOnly ? "YES" : "NO")
              << ", Strategy=" << getStrategyName(config.strategy);
    if (strategyUsesWeight(config.strategy)) {
        std::cout << ", Weight=" << config.weight;
    }
    std::cout << ", MaxNodes=" << config.maxNodes
              << ", Pruning=" << (config.usePruning ? "ON" : "OFF");
    if (config.usePruning) {
        std::cout << ", DeadlockLevel=" << config.deadlockLevel;
    }
    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) {
        std::cout << ", AnytimeMinWeight=" << config.anytimeMinWeight
                  << ", AnytimeDecay=" << config.anytimeWeightDecay;
    } else if (config.strategy == SolveStrategy::PortfolioSearch) {
        std::cout << ", PortfolioPassCount=" << config.portfolioPassCount
                  << ", PortfolioTogglePruning=" << (config.portfolioTogglePruning ? "ON" : "OFF");
    } else if (config.strategy == SolveStrategy::ARAStar) {
        std::cout << ", ARAMinEpsilon=" << config.araMinEpsilon
                  << ", ARADecay=" << config.araDecay;
    } else if (config.strategy == SolveStrategy::MHAStar) {
        std::cout << ", MHASecondaryWeight=" << config.mhaSecondaryWeight
                  << ", MHAAnchorBias=" << config.mhaAnchorBias;
    }
    std::cout << std::endl;
    std::cout << "------------------------------------" << std::endl;

    // --- 2. 加载关卡 ---
    std::string jsPath = "../js/levels.js";
    std::vector<RawLevel> carrotLevels;
    std::vector<RawLevel> eggLevels;

    if (!LevelLoader::loadFromJS(jsPath, carrotLevels, eggLevels)) {
        std::cerr << "Failed to load levels!" << std::endl;
        std::cin.get();
        return 1;
    }

    GameSolver solver;
    std::ofstream outFile("../js/solutions.js");
    if (!outFile.is_open()) {
        std::cerr << "Error: Cannot write to file." << std::endl;
        std::cin.get();
        return 1;
    }

    outFile << "// Auto-generated by CppSolver\n";
    outFile << "// Strategy: " << getStrategyName(config.strategy)
            << ", MaxNodes=" << config.maxNodes
            << ", Pruning=" << (config.usePruning ? "ON" : "OFF");
    if (config.usePruning) {
        outFile << ", DeadlockLevel=" << config.deadlockLevel;
    }
    outFile << ", DifficultOnly=" << (difficultOnly ? "YES" : "NO");
    if (strategyUsesWeight(config.strategy)) {
        outFile << ", Weight=" << config.weight;
    }
    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) {
        outFile << ", AnytimeMinWeight=" << config.anytimeMinWeight
                << ", AnytimeDecay=" << config.anytimeWeightDecay;
    } else if (config.strategy == SolveStrategy::PortfolioSearch) {
        outFile << ", PortfolioPassCount=" << config.portfolioPassCount
                << ", PortfolioTogglePruning=" << (config.portfolioTogglePruning ? "ON" : "OFF");
    } else if (config.strategy == SolveStrategy::ARAStar) {
        outFile << ", ARAMinEpsilon=" << config.araMinEpsilon
                << ", ARADecay=" << config.araDecay;
    } else if (config.strategy == SolveStrategy::MHAStar) {
        outFile << ", MHASecondaryWeight=" << config.mhaSecondaryWeight
                << ", MHAAnchorBias=" << config.mhaAnchorBias;
    }
    outFile << "\n";
    if (difficultOnly) {
        outFile << "// Difficult egg subset: 5,6,7,16,17,18,19,20\n";
    }

    outFile << "const AUTO_SOLVED_PATHS = {\n  carrot: {\n";

    std::vector<int> failedCarrotLevels;
    std::vector<int> failedEggLevels;
    std::vector<int> skippedEggLevels;

    // --- 3. 求解胡萝卜关卡 ---
    for (size_t i = 0; i < carrotLevels.size(); ++i) {
        if (difficultOnly) {
            outFile << "    " << i << ": \"\",\n";
            continue;
        }

        std::cout << "Level " << (i + 1) << "... " << std::flush;
        auto start = std::chrono::high_resolution_clock::now();
        std::string solution = solver.solve(carrotLevels[i].rows, config);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        if (!solution.empty()) {
            std::cout << "Solved! (" << solution.length() << " steps, " << elapsed.count() << "s)" << std::endl;
            outFile << "    " << i << ": \"" << solution << "\",\n";
        } else {
            std::cout << "Failed (Timeout or No Solution)." << std::endl;
            outFile << "    " << i << ": \"\",\n";
            failedCarrotLevels.push_back((int)i + 1);
        }
    }
    outFile << "  },\n  egg: {\n";

    // --- 4. 求解彩蛋关卡 ---
    for (size_t i = 0; i < eggLevels.size(); ++i) {
        size_t levelNo = i + 1;
        if (difficultOnly && !isDifficultEggLevel(levelNo)) {
            outFile << "    " << i << ": \"\",\n";
            skippedEggLevels.push_back((int)levelNo);
            continue;
        }

        std::cout << "Egg Level " << levelNo << "... " << std::flush;
        auto start = std::chrono::high_resolution_clock::now();
        std::string solution = solver.solve(eggLevels[i].rows, config);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        if (!solution.empty()) {
            std::cout << "Solved! (" << solution.length() << " steps, " << elapsed.count() << "s)" << std::endl;
            outFile << "    " << i << ": \"" << solution << "\",\n";
        } else {
            std::cout << "Failed." << std::endl;
            outFile << "    " << i << ": \"\",\n";
            failedEggLevels.push_back((int)levelNo);
        }
    }
    outFile << "  }\n};\n";

    outFile.close();

    std::cout << "\n=== Solve Summary ===" << std::endl;
    if (difficultOnly) {
        std::cout << "Carrot levels skipped due to difficult-only mode." << std::endl;
    } else {
        printLevelList("Carrot failed levels", failedCarrotLevels);
    }
    printLevelList("Egg failed levels", failedEggLevels);
    if (difficultOnly) {
        printLevelList("Egg skipped levels", skippedEggLevels);
    }

    std::cout << "\nAll done! Press Enter to exit...";
    std::cin.get();

    return 0;
}

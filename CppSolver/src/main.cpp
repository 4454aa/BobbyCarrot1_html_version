#include "../include/LevelLoader.h"
#include "../include/Solver.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

void applyCliOverrides(int argc, char** argv, SolverConfig& config) {
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
            else config.strategy = SolveStrategy::WeightedAStar;
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
    }
}

int main(int argc, char** argv) {
    // --- 1. 用户配置 ---
    std::cout << "=== Bobby's Solver Configuration ===" << std::endl;

    SolverConfig config;
    applyCliOverrides(argc, argv, config);

    int strategyDefault = 0;
    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) strategyDefault = 1;
    else if (config.strategy == SolveStrategy::PortfolioSearch) strategyDefault = 2;
    else if (config.strategy == SolveStrategy::GreedyBestFirst) strategyDefault = 3;

    std::cout << "Solve Strategy (0=Weighted A*, 1=Anytime Weighted A*, 2=Portfolio Search, 3=Greedy Best-First) [Default: " << strategyDefault << "]: ";
    std::string strategyLine;
    std::getline(std::cin, strategyLine);
    if (!strategyLine.empty()) {
        if (strategyLine == "1") config.strategy = SolveStrategy::AnytimeWeightedAStar;
        else if (strategyLine == "2") config.strategy = SolveStrategy::PortfolioSearch;
        else if (strategyLine == "3") config.strategy = SolveStrategy::GreedyBestFirst;
        else config.strategy = SolveStrategy::WeightedAStar;
    }

    config.weight = getUserInput<double>("Weighted A* Weight (1.0 = optimal, >1.0 = faster)", config.weight);
    config.maxNodes = getUserInput<int>("Max Nodes Limit", config.maxNodes);

    std::cout << "Use Reachability Pruning? (0=No, 1=Yes) [Default: " << (config.usePruning ? 1 : 0) << "]: ";
    std::string line;
    std::getline(std::cin, line);
    if (!line.empty()) {
        config.usePruning = line != "0";
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
    }

    std::cout << "------------------------------------" << std::endl;
    std::cout << "Config: Strategy=";
    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) std::cout << "Anytime Weighted A*";
    else if (config.strategy == SolveStrategy::PortfolioSearch) std::cout << "Portfolio Search";
    else if (config.strategy == SolveStrategy::GreedyBestFirst) std::cout << "Greedy Best-First";
    else std::cout << "Weighted A*";
    std::cout << ", Weight=" << config.weight
              << ", MaxNodes=" << config.maxNodes
              << ", Pruning=" << (config.usePruning ? "ON" : "OFF");
    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) {
        std::cout << ", AnytimeMinWeight=" << config.anytimeMinWeight
                  << ", AnytimeDecay=" << config.anytimeWeightDecay;
    } else if (config.strategy == SolveStrategy::PortfolioSearch) {
        std::cout << ", PortfolioPassCount=" << config.portfolioPassCount
                  << ", PortfolioTogglePruning=" << (config.portfolioTogglePruning ? "ON" : "OFF");
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

    outFile << "const AUTO_SOLVED_PATHS = {\n  carrot: {\n";

    // --- 3. 求解胡萝卜关卡 ---
    for (size_t i = 0; i < carrotLevels.size(); ++i) {
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
        }
    }
    outFile << "  },\n  egg: {\n";

    // --- 4. 求解彩蛋关卡 ---
    for (size_t i = 0; i < eggLevels.size(); ++i) {
        std::cout << "Egg Level " << (i + 1) << "... " << std::flush;
        std::string solution = solver.solve(eggLevels[i].rows, config);

        if (!solution.empty()) {
            std::cout << "Solved! (" << solution.length() << " steps)" << std::endl;
            outFile << "    " << i << ": \"" << solution << "\",\n";
        } else {
            std::cout << "Failed." << std::endl;
            outFile << "    " << i << ": \"\",\n";
        }
    }
    outFile << "  }\n};\n";

    outFile.close();
    std::cout << "\nAll done! Press Enter to exit...";
    std::cin.get();

    return 0;
}

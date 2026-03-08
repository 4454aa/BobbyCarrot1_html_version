#include "../include/LevelLoader.h"
#include "../include/Solver.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <sstream>

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

int main() {
    // --- 1. 用户配置 ---
    std::cout << "=== Bobby's Solver Configuration ===" << std::endl;

    std::cout << "Solve Strategy (0=Weighted A*, 1=Anytime Weighted A*) [Default: 0]: ";
    std::string strategyLine;
    std::getline(std::cin, strategyLine);

    SolverConfig config;
    if (!strategyLine.empty() && strategyLine != "0") {
        config.strategy = SolveStrategy::AnytimeWeightedAStar;
    }

    config.weight = getUserInput<double>("Weighted A* Weight (1.0 = optimal, >1.0 = faster)", 1.0);
    config.maxNodes = getUserInput<int>("Max Nodes Limit", 2000000);
    
    // 特殊处理 bool 输入
    std::cout << "Use Reachability Pruning? (0=No, 1=Yes) [Default: 0]: ";
    std::string line;
    std::getline(std::cin, line);
    bool usePruning = false;
    if (!line.empty() && line != "0") usePruning = true;
    config.usePruning = usePruning;

    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) {
        config.anytimeMinWeight = getUserInput<double>("Anytime min weight (>=1.0)", 1.0);
        config.anytimeWeightDecay = getUserInput<double>("Anytime weight decay (0~1, e.g. 0.5)", 0.5);
    }
    
    std::cout << "------------------------------------" << std::endl;
    std::cout << "Config: Strategy="
              << (config.strategy == SolveStrategy::AnytimeWeightedAStar ? "Anytime Weighted A*" : "Weighted A*")
              << ", Weight=" << config.weight
              << ", MaxNodes=" << config.maxNodes
              << ", Pruning=" << (config.usePruning ? "ON" : "OFF");
    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) {
        std::cout << ", AnytimeMinWeight=" << config.anytimeMinWeight
                  << ", AnytimeDecay=" << config.anytimeWeightDecay;
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
        
        // 调用带参数的 solve
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
        // 同样的参数
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

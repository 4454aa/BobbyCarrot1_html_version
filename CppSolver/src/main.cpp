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
    
    double weight = getUserInput<double>("Weighted A* Weight (1.0 = optimal, >1.0 = faster)", 1.0);
    int maxNodes = getUserInput<int>("Max Nodes Limit", 2000000);
    
    // 特殊处理 bool 输入
    std::cout << "Use Reachability Pruning? (0=No, 1=Yes) [Default: 0]: ";
    std::string line;
    std::getline(std::cin, line);
    bool usePruning = false;
    if (!line.empty() && line != "0") usePruning = true;
    
    std::cout << "------------------------------------" << std::endl;
    std::cout << "Config: Weight=" << weight << ", MaxNodes=" << maxNodes << ", Pruning=" << (usePruning ? "ON" : "OFF") << std::endl;
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
        std::string solution = solver.solve(carrotLevels[i].rows, weight, maxNodes, usePruning);
        
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
        std::string solution = solver.solve(eggLevels[i].rows, weight, maxNodes, usePruning);
        
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
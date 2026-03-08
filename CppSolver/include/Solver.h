#pragma once
#include "GameDef.h"
#include <string>
#include <vector>

enum class SolveStrategy {
    WeightedAStar,
    AnytimeWeightedAStar,
    PortfolioSearch
};

struct SolverConfig {
    double weight = 1.0;
    int maxNodes = 2000000;
    bool usePruning = false;
    SolveStrategy strategy = SolveStrategy::WeightedAStar;

    // Anytime Weighted A* 参数
    double anytimeMinWeight = 1.0;
    double anytimeWeightDecay = 0.5;

    // Portfolio Search 参数
    int portfolioPassCount = 4;
    bool portfolioTogglePruning = true;
};

class GameSolver {
public:
    std::string solve(const std::vector<std::string>& rawMap,
                      double weight,
                      int maxNodes,
                      bool usePruning);

    std::string solve(const std::vector<std::string>& rawMap,
                      const SolverConfig& config);

private:
    int ROWS, COLS;
    int TOTAL_CARROTS, TOTAL_EGGS;

    struct SearchResult {
        std::string path;
        int processed = 0;
        bool reachedNodeLimit = false;
    };

    // 核心逻辑
    int heuristic(const State& s, double weight);
    bool tryMove(const State& cur, int dx, int dy, State& next);

    SearchResult runWeightedAStar(const State& startS, double weight, int nodeBudget, bool usePruning);
    std::string runAnytimeWeightedAStar(const State& startS, const SolverConfig& config);
    std::string runPortfolioSearch(const State& startS, const SolverConfig& config);

    // 可达性剪枝
    bool checkReachability(const State& s);

    // 辅助函数
    bool isCorner(Tile t);
    Tile rotateCorner(Tile t);
    uint8_t getEntrySide(int dx, int dy);
    bool canExitCorner(Tile t, uint8_t entrySide, int dx, int dy);
    bool canEnterCorner(Tile t, int dx, int dy);
    bool getBeltDir(Tile t, int& dx, int& dy);
    void triggerRed(std::vector<Tile>& m);
    void triggerYellow(std::vector<Tile>& m);
};

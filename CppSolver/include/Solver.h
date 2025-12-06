#pragma once
#include "GameDef.h"
#include <string>
#include <vector>

class GameSolver {
public:
    std::string solve(const std::vector<std::string>& rawMap, 
                      double weight, 
                      int maxNodes, 
                      bool usePruning);

private:
    int ROWS, COLS;
    int TOTAL_CARROTS, TOTAL_EGGS;
    
    // 核心逻辑
    int heuristic(const State& s, double weight); // 增加 weight 参数
    bool tryMove(const State& cur, int dx, int dy, State& next);
    
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
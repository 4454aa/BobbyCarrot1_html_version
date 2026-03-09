#include "../include/Solver.h"
#include "../include/Utils.h"
#include <queue>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <iostream>

// 用于优先队列的简单封装
struct NodeWrapper {
    int idx; // 在 statePool 中的索引
    int f;   // f(n) = g(n) + h(n)
    bool operator>(const NodeWrapper& other) const { return f > other.f; }
};

// =========================================================
// 辅助逻辑实现 (Private Helpers)
// =========================================================

bool GameSolver::isCorner(Tile t) {
    return t >= Tiles::CORNER_RU && t <= Tiles::CORNER_LU;
}

Tile GameSolver::rotateCorner(Tile t) {
    if (t == Tiles::CORNER_RU) return Tiles::CORNER_RD;
    if (t == Tiles::CORNER_RD) return Tiles::CORNER_LD;
    if (t == Tiles::CORNER_LD) return Tiles::CORNER_LU;
    if (t == Tiles::CORNER_LU) return Tiles::CORNER_RU;
    return t;
}

// 0=None, 1=L, 2=R, 3=U, 4=D (相对于格子中心)
uint8_t GameSolver::getEntrySide(int dx, int dy) {
    if (dx == 1) return 1; // From Left
    if (dx == -1) return 2; // From Right
    if (dy == 1) return 3; // From Up
    if (dy == -1) return 4; // From Down
    return 0;
}

bool GameSolver::canExitCorner(Tile t, uint8_t entrySide, int dx, int dy) {
    // 1. Backtracking Logic (允许原路返回)
    if (entrySide == 1 && dx == -1) return true; // Came from L, going L
    if (entrySide == 2 && dx == 1) return true;  // Came from R, going R
    if (entrySide == 3 && dy == -1) return true; // Came from U, going U
    if (entrySide == 4 && dy == 1) return true;  // Came from D, going D

    // 2. Normal Flow Logic
    if (t == Tiles::CORNER_RU) return (entrySide == 1 && dy == 1) || (entrySide == 4 && dx == -1);
    if (t == Tiles::CORNER_RD) return (entrySide == 1 && dy == -1) || (entrySide == 3 && dx == -1);
    if (t == Tiles::CORNER_LD) return (entrySide == 2 && dy == -1) || (entrySide == 3 && dx == 1);
    if (t == Tiles::CORNER_LU) return (entrySide == 2 && dy == 1) || (entrySide == 4 && dx == 1);
    return false;
}

bool GameSolver::canEnterCorner(Tile t, int dx, int dy) {
    uint8_t side = getEntrySide(dx, dy);
    if (t == Tiles::CORNER_RU) return side == 1 || side == 4;
    if (t == Tiles::CORNER_RD) return side == 1 || side == 3;
    if (t == Tiles::CORNER_LD) return side == 2 || side == 3;
    if (t == Tiles::CORNER_LU) return side == 2 || side == 4;
    return false;
}

bool GameSolver::getBeltDir(Tile t, int& dx, int& dy) {
    if (t == Tiles::BELT_L) { dx = -1; dy = 0; return true; }
    if (t == Tiles::BELT_R) { dx = 1; dy = 0; return true; }
    if (t == Tiles::BELT_U) { dx = 0; dy = -1; return true; }
    if (t == Tiles::BELT_D) { dx = 0; dy = 1; return true; }
    return false;
}

void GameSolver::triggerRed(std::vector<Tile>& m) {
    for (auto& t : m) {
        if (t == Tiles::BTN_R) t = Tiles::BTN_r;
        else if (t == Tiles::BTN_r) t = Tiles::BTN_R;
        else if (t == Tiles::WALL_V) t = Tiles::WALL_H;
        else if (t == Tiles::WALL_H) t = Tiles::WALL_V;
        else if (isCorner(t)) t = rotateCorner(t);
    }
}

void GameSolver::triggerYellow(std::vector<Tile>& m) {
    for (auto& t : m) {
        if (t == Tiles::BTN_Y) t = Tiles::BTN_y;
        else if (t == Tiles::BTN_y) t = Tiles::BTN_Y;
        else if (t == Tiles::BELT_L) t = Tiles::BELT_R;
        else if (t == Tiles::BELT_R) t = Tiles::BELT_L;
        else if (t == Tiles::BELT_U) t = Tiles::BELT_D;
        else if (t == Tiles::BELT_D) t = Tiles::BELT_U;
    }
}

bool GameSolver::checkDeadlockLevel1(const State& s) {
    int remaining = (TOTAL_CARROTS - s.carrotsCollected) + (TOTAL_EGGS - s.eggsPlanted);
    if (remaining == 0) return true; 

    std::vector<bool> visited(ROWS * COLS, false);
    std::queue<int> q;
    
    int startIdx = s.y * COLS + s.x;
    q.push(startIdx);
    visited[startIdx] = true;
    
    int reachedCount = 0;
    int dirs[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};

    while(!q.empty()) {
        int curr = q.front();
        q.pop();
        
        Tile t = s.mapData[curr];
        if (t == Tiles::CARROT || t == Tiles::EGG_SPOT) {
            reachedCount++;
        }
        if (reachedCount >= remaining) return true;

        int cx = curr % COLS;
        int cy = curr / COLS;

        for(auto& d : dirs) {
            int nx = cx + d[0];
            int ny = cy + d[1];
            
            if(nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS) {
                int nIdx = ny * COLS + nx;
                if(visited[nIdx]) continue;
                
                Tile nt = s.mapData[nIdx];
                bool blocked = false;
                
                if (nt == Tiles::FENCE || nt == Tiles::GRASS || nt == Tiles::EGG_PLANTED || nt == Tiles::TRAP_ON) blocked = true;
                if (nt == Tiles::LOCK_S && !s.hasS) blocked = true;
                if (nt == Tiles::LOCK_G && !s.hasG) blocked = true;
                if (nt == Tiles::LOCK_C && !s.hasC) blocked = true;
                
                if (!blocked) {
                    visited[nIdx] = true;
                    q.push(nIdx);
                }
            }
        }
    }
    
    return false;
}

bool GameSolver::checkDeadlockLevel2(const State& s) {
    if (!checkDeadlockLevel1(s)) return false;

    int remaining = (TOTAL_CARROTS - s.carrotsCollected) + (TOTAL_EGGS - s.eggsPlanted);

    std::vector<bool> visited(ROWS * COLS, false);
    std::queue<int> q;
    int startIdx = s.y * COLS + s.x;
    q.push(startIdx);
    visited[startIdx] = true;

    bool keySReachable = s.hasS;
    bool keyGReachable = s.hasG;
    bool keyCReachable = s.hasC;
    bool hasLockS = false, hasLockG = false, hasLockC = false;
    int endIdx = -1;

    for (int i = 0; i < (int)s.mapData.size(); ++i) {
        if (s.mapData[i] == Tiles::LOCK_S) hasLockS = true;
        else if (s.mapData[i] == Tiles::LOCK_G) hasLockG = true;
        else if (s.mapData[i] == Tiles::LOCK_C) hasLockC = true;
        else if (s.mapData[i] == Tiles::END) endIdx = i;
    }

    int dirs[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};
    while (!q.empty()) {
        int curr = q.front();
        q.pop();

        Tile t = s.mapData[curr];
        if (t == Tiles::KEY_S) keySReachable = true;
        if (t == Tiles::KEY_G) keyGReachable = true;
        if (t == Tiles::KEY_C) keyCReachable = true;

        int cx = curr % COLS;
        int cy = curr / COLS;
        for (auto& d : dirs) {
            int nx = cx + d[0], ny = cy + d[1];
            if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) continue;
            int nIdx = ny * COLS + nx;
            if (visited[nIdx]) continue;

            Tile nt = s.mapData[nIdx];
            bool blocked = false;
            if (nt == Tiles::FENCE || nt == Tiles::GRASS || nt == Tiles::EGG_PLANTED || nt == Tiles::TRAP_ON) blocked = true;
            if (nt == Tiles::LOCK_S && !s.hasS) blocked = true;
            if (nt == Tiles::LOCK_G && !s.hasG) blocked = true;
            if (nt == Tiles::LOCK_C && !s.hasC) blocked = true;

            if (!blocked) {
                visited[nIdx] = true;
                q.push(nIdx);
            }
        }
    }

    if (!s.hasS && hasLockS && !keySReachable) return false;
    if (!s.hasG && hasLockG && !keyGReachable) return false;
    if (!s.hasC && hasLockC && !keyCReachable) return false;

    if (remaining == 0 && endIdx >= 0 && !visited[endIdx]) return false;

    return true;
}

// =========================================================
// 核心逻辑 (Heuristic & TryMove)
// =========================================================

int GameSolver::heuristic(const State& s, double weight) {
    int remaining = (TOTAL_CARROTS - s.carrotsCollected) + (TOTAL_EGGS - s.eggsPlanted);
    if (remaining == 0) {
        for (size_t i = 0; i < s.mapData.size(); ++i) {
            if (s.mapData[i] == Tiles::END) {
                int ox = i % COLS, oy = i / COLS;
                return (std::abs(s.x - ox) + std::abs(s.y - oy));
            }
        }
        return 0; 
    }

    int minDist = 9999;
    bool found = false;
    for (size_t i = 0; i < s.mapData.size(); ++i) {
        Tile t = s.mapData[i];
        if (t == Tiles::CARROT || t == Tiles::EGG_SPOT) {
            int tx = i % COLS, ty = i / COLS;
            int dist = std::abs(s.x - tx) + std::abs(s.y - ty);
            if (dist < minDist) minDist = dist;
            found = true;
        }
    }
    
    double h = (found ? minDist : 0) + (remaining * 3);
    return static_cast<int>(h * weight);
}

bool GameSolver::tryMove(const State& cur, int dx, int dy, State& next) {
    int nx = cur.x + dx;
    int ny = cur.y + dy;
    
    // 1. 越界检查
    if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) return false;

    int pIdx = cur.y * COLS + cur.x; 
    int nIdx = ny * COLS + nx;       
    
    Tile curCh = cur.mapData[pIdx];
    Tile nextCh = cur.mapData[nIdx];

    // 2. 离开检查 (Exit Check)
    if (isCorner(curCh)) {
        if (!canExitCorner(curCh, cur.entrySide, dx, dy)) return false;
    }
    if (curCh == Tiles::WALL_V && dx != 0) return false;
    if (curCh == Tiles::WALL_H && dy != 0) return false;

    // 3. 进入检查 (Entry Check)
    bool canEnter = true;
    if (nextCh == Tiles::FENCE || nextCh == Tiles::GRASS || nextCh == Tiles::EGG_PLANTED) canEnter = false;
    if (nextCh == Tiles::WALL_V && dx != 0) canEnter = false;
    if (nextCh == Tiles::WALL_H && dy != 0) canEnter = false;
    if (nextCh == Tiles::LOCK_S && !cur.hasS) canEnter = false;
    if (nextCh == Tiles::LOCK_G && !cur.hasG) canEnter = false;
    if (nextCh == Tiles::LOCK_C && !cur.hasC) canEnter = false;
    
    // 如果目标格是传送带，且方向与我移动方向完全相反，则视为墙壁
    if (nextCh == Tiles::BELT_L && dx == 1) canEnter = false;
    if (nextCh == Tiles::BELT_R && dx == -1) canEnter = false; 
    if (nextCh == Tiles::BELT_U && dy == 1) canEnter = false; 
    if (nextCh == Tiles::BELT_D && dy == -1) canEnter = false;

    if (isCorner(nextCh)) {
        if (!canEnterCorner(nextCh, dx, dy)) canEnter = false;
    }

    if (!canEnter) return false;

    // === 执行移动 ===
    next = cur; 
    next.steps++;
    next.parentIdx = -1; 
    next.moveChar = (dx == 1) ? 'R' : (dx == -1) ? 'L' : (dy == 1) ? 'D' : 'U';

    next.x = nx;
    next.y = ny;

    // 后结算 (Post-Move Effects)
    if (isCorner(curCh)) next.mapData[pIdx] = rotateCorner(curCh);
    else if (curCh == Tiles::WALL_V) next.mapData[pIdx] = Tiles::WALL_H;
    else if (curCh == Tiles::WALL_H) next.mapData[pIdx] = Tiles::WALL_V;
    else if (curCh == Tiles::TRAP) next.mapData[pIdx] = Tiles::TRAP_ON;
    else if (curCh == Tiles::EGG_SPOT) {
        next.mapData[pIdx] = Tiles::EGG_PLANTED;
        next.eggsPlanted++;
    }

    if (isCorner(nextCh)) next.entrySide = getEntrySide(dx, dy);
    else next.entrySide = 0;

    auto handleEnter = [&](int idx, Tile t) -> bool {
        if (t == Tiles::CARROT) {
            next.carrotsCollected++;
            next.mapData[idx] = Tiles::GROUND;
        } else if (t == Tiles::KEY_S) { next.hasS = true; next.mapData[idx] = Tiles::GROUND; }
        else if (t == Tiles::KEY_G) { next.hasG = true; next.mapData[idx] = Tiles::GROUND; }
        else if (t == Tiles::KEY_C) { next.hasC = true; next.mapData[idx] = Tiles::GROUND; }
        else if (t == Tiles::LOCK_S || t == Tiles::LOCK_G || t == Tiles::LOCK_C) next.mapData[idx] = Tiles::GROUND;
        else if (t == Tiles::BTN_R) triggerRed(next.mapData);
        else if (t == Tiles::BTN_Y) triggerYellow(next.mapData);
        else if (t == Tiles::TRAP_ON) return false; 
        return true;
    };

    if (!handleEnter(nIdx, nextCh)) return false; 

    // Belt Logic
    int loopC = 0;
    while (loopC < 500) { 
        Tile beltT = next.mapData[next.y * COLS + next.x];
        int bdx = 0, bdy = 0;
        if (!getBeltDir(beltT, bdx, bdy)) break;

        int bx = next.x + bdx;
        int by = next.y + bdy;
        
        if (bx < 0 || bx >= COLS || by < 0 || by >= ROWS) break; 
        
        Tile bNext = next.mapData[by * COLS + bx];
        bool blocked = false;
        
        if (bNext == Tiles::FENCE || bNext == Tiles::GRASS || bNext == Tiles::EGG_PLANTED) blocked = true;
        if (bNext == Tiles::WALL_V || bNext == Tiles::WALL_H) blocked = true;
        if ((bNext == Tiles::LOCK_S && !next.hasS) || 
            (bNext == Tiles::LOCK_G && !next.hasG) || 
            (bNext == Tiles::LOCK_C && !next.hasC)) blocked = true;
        if (isCorner(bNext)) blocked = true; 

        if (blocked) break;

        next.x = bx;
        next.y = by;
        int bIdx = by * COLS + bx;
        
        if (!handleEnter(bIdx, bNext)) return false; 

        loopC++;
    }
    if (loopC >= 500) return false; 

    next.estimatedTotal = next.steps + heuristic(next, 1.0); 
    return true;
}


GameSolver::SearchResult GameSolver::runWeightedAStar(const State& startS, double weight, int nodeBudget, bool usePruning, int deadlockLevel) {
    SearchResult result;

    if (nodeBudget <= 0) {
        result.reachedNodeLimit = true;
        return result;
    }

    std::vector<State> statePool;
    statePool.reserve(1000000);

    State weightedStart = startS;
    weightedStart.estimatedTotal = weightedStart.steps + heuristic(weightedStart, weight);
    statePool.push_back(weightedStart);

    std::priority_queue<NodeWrapper, std::vector<NodeWrapper>, std::greater<NodeWrapper>> openSet;
    openSet.push({0, weightedStart.estimatedTotal});

    std::unordered_set<State, StateHash> closedSet;

    int dirs[4][2] = {{0,-1}, {0,1}, {-1,0}, {1,0}};

    while (!openSet.empty()) {
        NodeWrapper top = openSet.top();
        openSet.pop();
        State current = statePool[top.idx];

        if (closedSet.count(current)) continue;
        closedSet.insert(current);

        result.processed++;
        if (result.processed > nodeBudget) {
            result.reachedNodeLimit = true;
            return result;
        }

        if (current.carrotsCollected == TOTAL_CARROTS && current.eggsPlanted == TOTAL_EGGS) {
            if (current.mapData[current.y * COLS + current.x] == Tiles::END) {
                std::string path = "";
                int currIdx = top.idx;
                while (currIdx != 0) {
                    path += statePool[currIdx].moveChar;
                    currIdx = statePool[currIdx].parentIdx;
                }
                std::reverse(path.begin(), path.end());
                result.path = path;
                return result;
            }
        }

        for (auto& d : dirs) {
            State next;
            if (!tryMove(current, d[0], d[1], next)) continue;

            if (usePruning) {
                bool ok = (deadlockLevel >= 2) ? checkDeadlockLevel2(next) : checkDeadlockLevel1(next);
                if (!ok) continue;
            }

            if (closedSet.find(next) == closedSet.end()) {
                next.parentIdx = top.idx;
                next.estimatedTotal = next.steps + heuristic(next, weight);

                statePool.push_back(next);
                openSet.push({(int)statePool.size() - 1, next.estimatedTotal});
            }
        }
    }

    return result;
}

std::string GameSolver::runAnytimeWeightedAStar(const State& startS, const SolverConfig& config) {
    if (config.maxNodes <= 0) return "";

    double minWeight = std::max(1.0, config.anytimeMinWeight);
    double decay = config.anytimeWeightDecay;
    if (decay <= 0.0 || decay >= 1.0) decay = 0.5;

    double currentWeight = std::max(config.weight, minWeight);
    int remainingBudget = config.maxNodes;
    std::string bestPath;

    while (remainingBudget > 0) {
        SearchResult passResult = runWeightedAStar(startS, currentWeight, remainingBudget, config.usePruning, config.deadlockLevel);
        remainingBudget -= passResult.processed;

        if (!passResult.path.empty()) {
            if (bestPath.empty() || passResult.path.size() < bestPath.size()) {
                bestPath = passResult.path;
            }
        }

        if (!passResult.reachedNodeLimit && currentWeight <= minWeight + 1e-9) {
            break;
        }
        if (remainingBudget <= 0) break;

        if (currentWeight <= minWeight + 1e-9) {
            // 已经降到最小权重，继续重复同权重意义不大
            break;
        }

        currentWeight = std::max(minWeight, currentWeight * decay);
    }

    return bestPath;
}


std::string GameSolver::runPortfolioSearch(const State& startS, const SolverConfig& config) {
    if (config.maxNodes <= 0) return "";

    int passCount = std::max(1, config.portfolioPassCount);
    int remainingBudget = config.maxNodes;
    std::string bestPath;

    // 组合不同权重的加权 A*，提升“撞到解”的概率
    std::vector<double> weightPool = {
        std::max(1.0, config.weight + 2.0),
        std::max(1.0, config.weight + 1.0),
        std::max(1.0, config.weight),
        std::max(1.0, config.weight * 0.75),
        1.0
    };

    int runs = 0;
    for (double w : weightPool) {
        if (runs >= passCount || remainingBudget <= 0) break;

        int runsLeft = passCount - runs;
        int budgetForRun = std::max(1, remainingBudget / runsLeft);

        bool pruneFlag = config.usePruning;
        if (config.portfolioTogglePruning && (runs % 2 == 1)) {
            pruneFlag = !pruneFlag;
        }

        SearchResult res = runWeightedAStar(startS, w, budgetForRun, pruneFlag, config.deadlockLevel);
        remainingBudget -= std::min(remainingBudget, res.processed);

        if (!res.path.empty()) {
            if (bestPath.empty() || res.path.size() < bestPath.size()) {
                bestPath = res.path;
            }
        }

        runs++;
    }

    return bestPath;
}

std::string GameSolver::runGreedyBestFirst(const State& startS, const SolverConfig& config) {
    if (config.maxNodes <= 0) return "";

    std::vector<State> statePool;
    statePool.reserve(1000000);

    State greedyStart = startS;
    greedyStart.estimatedTotal = heuristic(greedyStart, std::max(1.0, config.weight));
    statePool.push_back(greedyStart);

    std::priority_queue<NodeWrapper, std::vector<NodeWrapper>, std::greater<NodeWrapper>> openSet;
    openSet.push({0, greedyStart.estimatedTotal});

    std::unordered_set<State, StateHash> closedSet;
    int dirs[4][2] = {{0,-1}, {0,1}, {-1,0}, {1,0}};
    int processed = 0;

    while (!openSet.empty()) {
        NodeWrapper top = openSet.top();
        openSet.pop();
        State current = statePool[top.idx];

        if (closedSet.count(current)) continue;
        closedSet.insert(current);

        processed++;
        if (processed > config.maxNodes) return "";

        if (current.carrotsCollected == TOTAL_CARROTS && current.eggsPlanted == TOTAL_EGGS) {
            if (current.mapData[current.y * COLS + current.x] == Tiles::END) {
                std::string path = "";
                int currIdx = top.idx;
                while (currIdx != 0) {
                    path += statePool[currIdx].moveChar;
                    currIdx = statePool[currIdx].parentIdx;
                }
                std::reverse(path.begin(), path.end());
                return path;
            }
        }

        for (auto& d : dirs) {
            State next;
            if (!tryMove(current, d[0], d[1], next)) continue;
            if (config.usePruning) {
                bool ok = (config.deadlockLevel >= 2) ? checkDeadlockLevel2(next) : checkDeadlockLevel1(next);
                if (!ok) continue;
            }

            if (closedSet.find(next) == closedSet.end()) {
                next.parentIdx = top.idx;
                // Greedy Best-First: 仅按 h 排序
                next.estimatedTotal = heuristic(next, std::max(1.0, config.weight));
                statePool.push_back(next);
                openSet.push({(int)statePool.size() - 1, next.estimatedTotal});
            }
        }
    }

    return "";
}

std::string GameSolver::solve(const std::vector<std::string>& rawMap, double weight, int maxNodes, bool usePruning) {
    SolverConfig config;
    config.weight = weight;
    config.maxNodes = maxNodes;
    config.usePruning = usePruning;
    config.strategy = SolveStrategy::WeightedAStar;
    return solve(rawMap, config);
}

std::string GameSolver::solve(const std::vector<std::string>& rawMap, const SolverConfig& config) {
    ROWS = rawMap.size();
    COLS = 0;
    TOTAL_CARROTS = 0; 
    TOTAL_EGGS = 0;

    std::vector<Tile> initialData;
    int startX = 0, startY = 0;

    // 解析
    for (int y = 0; y < ROWS; ++y) {
        int idx = 0; int x = 0;
        const std::string& rowStr = rawMap[y];
        while (idx < rowStr.size()) {
            Tile t = parseNextToken(rowStr, idx);
            if (t == Tiles::START) { startX = x; startY = y; t = Tiles::GROUND; }
            if (t == Tiles::CARROT) TOTAL_CARROTS++;
            if (t == Tiles::EGG_SPOT) TOTAL_EGGS++;
            initialData.push_back(t);
            x++;
        }
        if (y == 0) COLS = x;
    }

    State startS;
    startS.x = startX; startS.y = startY;
    startS.mapData = initialData;
    startS.estimatedTotal = startS.steps + heuristic(startS, config.weight);

    if (config.strategy == SolveStrategy::AnytimeWeightedAStar) {
        return runAnytimeWeightedAStar(startS, config);
    }
    if (config.strategy == SolveStrategy::PortfolioSearch) {
        return runPortfolioSearch(startS, config);
    }
    if (config.strategy == SolveStrategy::GreedyBestFirst) {
        return runGreedyBestFirst(startS, config);
    }
    return runWeightedAStar(startS, config.weight, config.maxNodes, config.usePruning, config.deadlockLevel).path;
}

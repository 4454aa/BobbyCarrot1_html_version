#pragma once
#include <vector>
#include <string>
#include <cstdint>

// 使用 uint8_t 节省内存
using Tile = uint8_t;

namespace Tiles {
    const Tile GRASS = 0;   const Tile GROUND = 1;
    const Tile START = 2;   const Tile END = 3;
    const Tile CARROT = 4;  const Tile FENCE = 5;
    const Tile TRAP = 6;    const Tile TRAP_ON = 7;
    const Tile BELT_L = 10; const Tile BELT_R = 11;
    const Tile BELT_U = 12; const Tile BELT_D = 13;
    const Tile WALL_V = 20; const Tile WALL_H = 21;
    const Tile CORNER_RU = 30; const Tile CORNER_RD = 31;
    const Tile CORNER_LD = 32; const Tile CORNER_LU = 33;
    const Tile BTN_R = 40;  const Tile BTN_r = 41;
    const Tile BTN_Y = 42;  const Tile BTN_y = 43;
    const Tile KEY_S = 50;  const Tile LOCK_S = 51;
    const Tile KEY_G = 52;  const Tile LOCK_G = 53;
    const Tile KEY_C = 54;  const Tile LOCK_C = 55;
    const Tile EGG_SPOT = 60; const Tile EGG_PLANTED = 61;
}

// 状态结构体
struct State {
    int x, y;
    std::vector<Tile> mapData;
    bool hasS = false, hasG = false, hasC = false;
    int carrotsCollected = 0;
    int eggsPlanted = 0;
    
    int steps = 0;          // g(n)
    int estimatedTotal = 0; // f(n)
    
    // 路径回溯信息
    int parentIdx = -1;
    char moveChar = 0;
    uint8_t entrySide = 0; // 0=None, 1=L, 2=R, 3=U, 4=D

    bool operator==(const State& other) const {
        if (x != other.x || y != other.y) return false;
        if (carrotsCollected != other.carrotsCollected || eggsPlanted != other.eggsPlanted) return false;
        if (hasS != other.hasS || hasG != other.hasG || hasC != other.hasC) return false;
        if (entrySide != other.entrySide) return false;
        return mapData == other.mapData;
    }
};
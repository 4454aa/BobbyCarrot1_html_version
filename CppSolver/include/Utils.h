#pragma once
#include "GameDef.h"
#include <string>

// 状态哈希函数
struct StateHash {
    size_t operator()(const State& s) const {
        size_t seed = 0;
        auto hash_combine = [&seed](size_t v) {
            seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
        hash_combine(s.x); hash_combine(s.y); hash_combine(s.entrySide);
        hash_combine((s.hasS << 2) | (s.hasG << 1) | s.hasC);
        hash_combine(s.carrotsCollected); hash_combine(s.eggsPlanted);
        
        // FNV-1a hash for map data
        size_t mapHash = 14695981039346656037ULL;
        for (auto t : s.mapData) {
            mapHash ^= t;
            mapHash *= 1099511628211ULL;
        }
        hash_combine(mapHash);
        return seed;
    }
};

// 字符解析辅助
inline bool matchMultibyte(const std::string& s, int idx, const std::string& target) {
    if (idx + target.size() > s.size()) return false;
    for (size_t i = 0; i < target.size(); ++i) {
        if (s[idx + i] != target[i]) return false;
    }
    return true;
}

inline Tile parseNextToken(const std::string& s, int& idx) {
    if (idx >= s.size()) return Tiles::GRASS;
    unsigned char c = s[idx];
    if (c < 128) {
        idx++;
        switch(c) {
            case '.': return Tiles::GRASS; case ' ': return Tiles::GROUND;
            case '@': return Tiles::START; case 'o': return Tiles::END;
            case '*': return Tiles::CARROT; case '=': return Tiles::FENCE;
            case 'x': return Tiles::TRAP;   case 'X': return Tiles::TRAP_ON;
            case '<': return Tiles::BELT_L; case '>': return Tiles::BELT_R;
            case '^': return Tiles::BELT_U; case 'v': return Tiles::BELT_D;
            case '|': return Tiles::WALL_V; case '-': return Tiles::WALL_H;
            case 'R': return Tiles::BTN_R;  case 'r': return Tiles::BTN_r;
            case 'Y': return Tiles::BTN_Y;  case 'y': return Tiles::BTN_y;
            case 's': return Tiles::KEY_S;  case 'S': return Tiles::LOCK_S;
            case 'g': return Tiles::KEY_G;  case 'G': return Tiles::LOCK_G;
            case 'c': return Tiles::KEY_C;  case 'C': return Tiles::LOCK_C;
            case 'e': return Tiles::EGG_SPOT; case 'E': return Tiles::EGG_PLANTED;
            default: return Tiles::GRASS;
        }
    } else {
        if (matchMultibyte(s, idx, "⌝")) { idx += 3; return Tiles::CORNER_RU; }
        if (matchMultibyte(s, idx, "⌟")) { idx += 3; return Tiles::CORNER_RD; }
        if (matchMultibyte(s, idx, "⌞")) { idx += 3; return Tiles::CORNER_LD; }
        if (matchMultibyte(s, idx, "⌜")) { idx += 3; return Tiles::CORNER_LU; }
        idx++; return Tiles::GRASS;
    }
}
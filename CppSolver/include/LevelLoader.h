#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

struct RawLevel {
    std::vector<std::string> rows;
};

class LevelLoader {
public:
    static bool loadFromJS(const std::string& filename, 
                           std::vector<RawLevel>& outCarrotLevels, 
                           std::vector<RawLevel>& outEggLevels) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open " << filename << std::endl;
            return false;
        }

        std::string line;
        bool inCarrotSection = false;
        bool inEggSection = false;
        
        RawLevel currentLevel;
        bool buildingLevel = false;

        while (std::getline(file, line)) {
            // 简单的状态机检测
            if (line.find("carrot_levels") != std::string::npos || line.find("CARROT_LEVELS") != std::string::npos) {
                inCarrotSection = true; inEggSection = false;
            } else if (line.find("egg_levels") != std::string::npos || line.find("EGG_LEVELS") != std::string::npos) {
                inEggSection = true; inCarrotSection = false;
            }

            // 提取双引号内的内容 "....."
            size_t firstQuote = line.find('"');
            size_t lastQuote = line.rfind('"');

            if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote) {
                std::string rowContent = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                currentLevel.rows.push_back(rowContent);
                buildingLevel = true;
            } else {
                // 如果当前行没有引号，且之前正在构建关卡，说明一个关卡结束了
                // JS 数组通常是 { "..." }, 或 [ "..." ],
                if (buildingLevel) {
                    if (inCarrotSection) outCarrotLevels.push_back(currentLevel);
                    else if (inEggSection) outEggLevels.push_back(currentLevel);
                    
                    currentLevel.rows.clear();
                    buildingLevel = false;
                }
            }
        }
        return true;
    }
};
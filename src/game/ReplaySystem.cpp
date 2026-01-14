//
// ReplaySystem.cpp - Replay recording and playback implementation
//

#include "ReplaySystem.h"
#include <sstream>
#include <filesystem>
#include <iostream>

ReplaySystem::~ReplaySystem() {
    StopRecording();
}

bool ReplaySystem::CreateDirectoryIfNeeded(const std::string& filepath) {
    std::filesystem::path path(filepath);
    std::filesystem::path dir = path.parent_path();

    if (dir.empty()) return true;

    if (!std::filesystem::exists(dir)) {
        try {
            std::filesystem::create_directories(dir);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to create directory: " << e.what() << std::endl;
            return false;
        }
    }
    return true;
}

bool ReplaySystem::StartRecording(const std::string& filepath, const GameConfig& config) {
    if (_isRecording) {
        StopRecording();
    }

    if (!CreateDirectoryIfNeeded(filepath)) {
        return false;
    }

    _outFile.open(filepath, std::ios::out | std::ios::trunc);
    if (!_outFile.is_open()) {
        std::cerr << "Failed to open replay file for writing: " << filepath << std::endl;
        return false;
    }

    _config = config;
    WriteConfig(config);

    _outFile << "\n[ACTIONS]\n";
    _outFile.flush();

    _isRecording = true;
    std::cout << "Recording replay to: " << filepath << std::endl;
    return true;
}

void ReplaySystem::WriteConfig(const GameConfig& config) {
    _outFile << "# Hex Empire Replay v1\n";
    _outFile << "[CONFIG]\n";
    _outFile << "gridWidth=" << config.gridWidth << "\n";
    _outFile << "gridHeight=" << config.gridHeight << "\n";
    _outFile << "playerCount=" << config.playerCount << "\n";
    _outFile << "humanPlayerIndex=" << config.humanPlayerIndex << "\n";
    _outFile << "targetTerritoryCount=" << config.targetTerritoryCount << "\n";
    _outFile << "minTerritorySize=" << config.minTerritorySize << "\n";
    _outFile << "maxTerritorySize=" << config.maxTerritorySize << "\n";
    _outFile << "startingDicePerPlayer=" << config.startingDicePerPlayer << "\n";
    _outFile << "hexSize=" << config.hexSize << "\n";
    _outFile << "seed=" << config.seed << "\n";
    _outFile << "fillHoles=" << (config.fillHoles ? 1 : 0) << "\n";
    _outFile << "minHoleSize=" << config.minHoleSize << "\n";
    _outFile << "keepLargestIslandOnly=" << (config.keepLargestIslandOnly ? 1 : 0) << "\n";
}

void ReplaySystem::RecordAction(const CombatAction& action) {
    if (!_isRecording || !_outFile.is_open()) return;

    _outFile << action.attackerId << ","
             << action.defenderId << ","
             << static_cast<int>(action.attackerPlayer) << ","
             << static_cast<int>(action.attackerDice) << ","
             << static_cast<int>(action.defenderDice) << "\n";
    _outFile.flush();
}

void ReplaySystem::StopRecording() {
    if (_isRecording && _outFile.is_open()) {
        _outFile.close();
        std::cout << "Replay recording stopped." << std::endl;
    }
    _isRecording = false;
}

bool ReplaySystem::LoadReplay(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open replay file: " << filepath << std::endl;
        return false;
    }

    _actions.clear();
    _currentActionIndex = 0;
    _config = GameConfig{};

    if (!ParseConfig(file)) {
        std::cerr << "Failed to parse replay config" << std::endl;
        return false;
    }

    if (!ParseActions(file)) {
        std::cerr << "Failed to parse replay actions" << std::endl;
        return false;
    }

    _isLoaded = true;
    std::cout << "Loaded replay: " << filepath << " (" << _actions.size() << " actions)" << std::endl;
    return true;
}

bool ReplaySystem::ParseConfig(std::ifstream& file) {
    std::string line;
    bool inConfig = false;

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        if (line == "[CONFIG]") {
            inConfig = true;
            continue;
        }

        if (line == "[ACTIONS]") {
            return true; // Done with config section
        }

        if (!inConfig) continue;

        // Parse key=value
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        if (key == "gridWidth") _config.gridWidth = std::stoi(value);
        else if (key == "gridHeight") _config.gridHeight = std::stoi(value);
        else if (key == "playerCount") _config.playerCount = std::stoi(value);
        else if (key == "humanPlayerIndex") _config.humanPlayerIndex = std::stoi(value);
        else if (key == "targetTerritoryCount") _config.targetTerritoryCount = std::stoi(value);
        else if (key == "minTerritorySize") _config.minTerritorySize = std::stoi(value);
        else if (key == "maxTerritorySize") _config.maxTerritorySize = std::stoi(value);
        else if (key == "startingDicePerPlayer") _config.startingDicePerPlayer = std::stoi(value);
        else if (key == "hexSize") _config.hexSize = std::stof(value);
        else if (key == "seed") _config.seed = std::stoul(value);
        else if (key == "fillHoles") _config.fillHoles = (std::stoi(value) != 0);
        else if (key == "minHoleSize") _config.minHoleSize = std::stoi(value);
        else if (key == "keepLargestIslandOnly") _config.keepLargestIslandOnly = (std::stoi(value) != 0);
    }

    return true;
}

bool ReplaySystem::ParseActions(std::ifstream& file) {
    std::string line;

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        // Parse comma-separated values: attackerId,defenderId,attackerPlayer,attackerDice,defenderDice
        std::stringstream ss(line);
        std::string token;
        std::vector<int> values;

        while (std::getline(ss, token, ',')) {
            try {
                values.push_back(std::stoi(token));
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse action line: " << line << std::endl;
                return false;
            }
        }

        if (values.size() != 5) {
            std::cerr << "Invalid action format: " << line << std::endl;
            continue;
        }

        CombatAction action;
        action.attackerId = static_cast<TerritoryId>(values[0]);
        action.defenderId = static_cast<TerritoryId>(values[1]);
        action.attackerPlayer = static_cast<PlayerId>(values[2]);
        action.attackerDice = static_cast<uint8_t>(values[3]);
        action.defenderDice = static_cast<uint8_t>(values[4]);

        _actions.push_back(action);
    }

    return true;
}

bool ReplaySystem::HasNextAction() const {
    return _isLoaded && _currentActionIndex < _actions.size();
}

CombatAction ReplaySystem::GetNextAction() {
    if (!HasNextAction()) {
        return CombatAction{};
    }
    return _actions[_currentActionIndex++];
}

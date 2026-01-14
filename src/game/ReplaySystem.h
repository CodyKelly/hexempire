//
// ReplaySystem.h - Replay recording and playback
//

#ifndef ATLAS_REPLAYSYSTEM_H
#define ATLAS_REPLAYSYSTEM_H

#include "GameData.h"
#include <string>
#include <vector>
#include <fstream>

class ReplaySystem {
public:
    ReplaySystem() = default;
    ~ReplaySystem();

    // Writing (recording mode)
    bool StartRecording(const std::string& filepath, const GameConfig& config);
    void RecordAction(const CombatAction& action);
    void StopRecording();

    // Reading (playback mode)
    bool LoadReplay(const std::string& filepath);
    [[nodiscard]] const GameConfig& GetConfig() const { return _config; }
    [[nodiscard]] bool HasNextAction() const;
    CombatAction GetNextAction();
    [[nodiscard]] size_t GetActionCount() const { return _actions.size(); }
    [[nodiscard]] size_t GetCurrentActionIndex() const { return _currentActionIndex; }

    // State
    [[nodiscard]] bool IsRecording() const { return _isRecording; }
    [[nodiscard]] bool IsLoaded() const { return _isLoaded; }

private:
    std::ofstream _outFile;
    GameConfig _config;
    std::vector<CombatAction> _actions;
    size_t _currentActionIndex = 0;
    bool _isRecording = false;
    bool _isLoaded = false;

    void WriteConfig(const GameConfig& config);
    bool ParseConfig(std::ifstream& file);
    bool ParseActions(std::ifstream& file);
    static bool CreateDirectoryIfNeeded(const std::string& filepath);
};

#endif // ATLAS_REPLAYSYSTEM_H

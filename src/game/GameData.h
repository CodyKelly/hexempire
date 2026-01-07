//
// GameData.h - Core game state data structures
//

#ifndef ATLAS_GAMEDATA_H
#define ATLAS_GAMEDATA_H

#include "../hex/HexCoord.h"
#include <array>
#include <vector>
#include <unordered_map>
#include <string>

// Type aliases for clarity
using PlayerId = uint8_t;
using TerritoryId = uint16_t;

// Constants
constexpr PlayerId PLAYER_NONE = 255;
constexpr TerritoryId TERRITORY_NONE = 65535;
constexpr int MAX_PLAYERS = 8;
constexpr int MAX_DICE_PER_TERRITORY = 8;

// Player colors (distinct, colorblind-friendly palette)
constexpr std::array<std::array<float, 3>, MAX_PLAYERS> PLAYER_COLORS = {
    {
        {0.90f, 0.30f, 0.30f}, // Player 0: Red
        {0.30f, 0.60f, 0.90f}, // Player 1: Blue
        {0.30f, 0.80f, 0.40f}, // Player 2: Green
        {0.95f, 0.75f, 0.20f}, // Player 3: Yellow
        {0.70f, 0.40f, 0.80f}, // Player 4: Purple
        {0.95f, 0.55f, 0.25f}, // Player 5: Orange
        {0.50f, 0.80f, 0.85f}, // Player 6: Cyan
        {0.85f, 0.50f, 0.70f} // Player 7: Pink
    }
};

// Player data
struct PlayerData {
    PlayerId id = PLAYER_NONE;
    bool isHuman = false;
    bool isEliminated = false;
    float colorR = 0.5f;
    float colorG = 0.5f;
    float colorB = 0.5f;
    std::string name;

    void SetColorFromPalette() {
        if (id < MAX_PLAYERS) {
            colorR = PLAYER_COLORS[id][0];
            colorG = PLAYER_COLORS[id][1];
            colorB = PLAYER_COLORS[id][2];
        }
    }
};

// Territory data (a contiguous group of hexes)
struct TerritoryData {
    TerritoryId id = TERRITORY_NONE;
    PlayerId owner = PLAYER_NONE;
    uint8_t diceCount = 1;
    std::vector<HexCoord> hexes;
    std::vector<TerritoryId> neighbors;
    HexCoord centerHex;

    [[nodiscard]] bool IsOwnedBy(PlayerId player) const { return owner == player; }
    [[nodiscard]] bool CanAttack() const { return diceCount >= 2; }
};

// Combat result for display/animation
struct CombatResult {
    TerritoryId attackerId = TERRITORY_NONE;
    TerritoryId defenderId = TERRITORY_NONE;
    PlayerId attackerPlayer = PLAYER_NONE;
    PlayerId defenderPlayer = PLAYER_NONE;
    std::vector<int> attackerRolls;
    std::vector<int> defenderRolls;
    int attackerTotal = 0;
    int defenderTotal = 0;
    bool attackerWins = false;
};

// Turn phases
enum class TurnPhase {
    SelectAttacker, // Human selecting territory to attack from
    SelectTarget, // Human selecting target territory
    Resolving, // Combat animation/resolution in progress
    AITurn, // AI player making decisions
    Reinforcement, // End-of-turn dice distribution
    GameOver // Victory condition met
};

// Game configuration
struct GameConfig {
    int gridRadius = 8; // Hex grid radius
    int playerCount = 8; // Number of players (2-8)
    int humanPlayerIndex = 0; // Which player is human
    int targetTerritoryCount = 48; // Target number of territories
    int minTerritorySize = 3; // Minimum hexes per territory
    int maxTerritorySize = 12; // Maximum hexes per territory
    int startingDicePerPlayer = 20; // Initial dice to distribute
    float hexSize = 24.0f; // Hex size in world units
    unsigned int seed = 0; // Random seed (0 = random)

    // Post-processing options
    bool fillHoles = false; // Fill small unassigned hex groups after territory generation
    int minHoleSize = 4; // Holes smaller than this get absorbed by neighbors
    bool keepLargestIslandOnly = false; // Remove all territory islands except the largest
};

// Complete game state
struct GameState {
    // Configuration
    GameConfig config;

    // Players
    std::array<PlayerData, MAX_PLAYERS> players;
    int activePlayerCount = 0;

    // Current turn
    PlayerId currentPlayer = 0;
    int turnNumber = 1;
    TurnPhase phase = TurnPhase::SelectAttacker;

    // Map data
    std::vector<TerritoryData> territories;
    std::unordered_map<HexCoord, TerritoryId, HexCoordHash> hexToTerritory;

    // Selection state (for human player)
    TerritoryId selectedTerritory = TERRITORY_NONE;
    std::vector<TerritoryId> validTargets;

    // Combat state
    CombatResult lastCombat;
    bool combatPending = false;
    float combatAnimTimer = 0.0f;

    // Map refresh flag - set when territory ownership changes
    bool mapNeedsRefresh = false;

    // Victory
    PlayerId winner = PLAYER_NONE;

    // Helper methods
    [[nodiscard]] bool IsGameOver() const { return winner != PLAYER_NONE; }

    [[nodiscard]] bool IsHumanTurn() const {
        return currentPlayer < MAX_PLAYERS &&
               players[currentPlayer].isHuman &&
               !players[currentPlayer].isEliminated;
    }

    [[nodiscard]] TerritoryData *GetTerritory(TerritoryId id) {
        if (id < territories.size()) return &territories[id];
        return nullptr;
    }

    [[nodiscard]] const TerritoryData *GetTerritory(TerritoryId id) const {
        if (id < territories.size()) return &territories[id];
        return nullptr;
    }

    [[nodiscard]] TerritoryId GetTerritoryAt(const HexCoord &coord) const {
        auto it = hexToTerritory.find(coord);
        if (it != hexToTerritory.end()) return it->second;
        return TERRITORY_NONE;
    }

    [[nodiscard]] const PlayerData &GetPlayer(PlayerId id) const {
        static PlayerData nullPlayer;
        if (id < MAX_PLAYERS) return players[id];
        return nullPlayer;
    }

    [[nodiscard]] int CountTerritoriesOwned(PlayerId player) const {
        int count = 0;
        for (const auto &t: territories) {
            if (t.owner == player) count++;
        }
        return count;
    }

    [[nodiscard]] int CountDiceOwned(PlayerId player) const {
        int count = 0;
        for (const auto &t: territories) {
            if (t.owner == player) count += t.diceCount;
        }
        return count;
    }
};

// UI state (separate from game state for clean separation)
struct UIState {
    // Hover
    HexCoord hoveredHex;
    TerritoryId hoveredTerritory = TERRITORY_NONE;
    bool isHovering = false;

    // Selection highlight hexes (for rendering)
    std::vector<HexCoord> selectedHexes;
    std::vector<HexCoord> validTargetHexes;
    std::vector<HexCoord> hoverHexes;

    // Combat display
    bool showCombatResult = false;
    float combatDisplayTimer = 0.0f;

    // End turn button (screen coords)
    float endTurnBtnX = 0;
    float endTurnBtnY = 0;
    float endTurnBtnW = 120;
    float endTurnBtnH = 40;
    bool endTurnHovered = false;
};

#endif // ATLAS_GAMEDATA_H

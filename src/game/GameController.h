//
// GameController.h - Game flow and turn management
//

#ifndef ATLAS_GAMECONTROLLER_H
#define ATLAS_GAMECONTROLLER_H

#include "GameData.h"
#include "CombatSystem.h"
#include "../hex/HexGrid.h"
#include "../hex/TerritoryGenerator.h"

class AIController;  // Forward declaration

class GameController
{
public:
    GameController();

    // Initialize a new game
    void InitializeGame(const GameConfig& config);

    // Set AI controller reference
    void SetAIController(AIController* ai) { _aiController = ai; }

    // Get game state
    [[nodiscard]] GameState& GetState() { return _state; }
    [[nodiscard]] const GameState& GetState() const { return _state; }

    // Get hex grid
    [[nodiscard]] HexGrid& GetGrid() { return _grid; }
    [[nodiscard]] const HexGrid& GetGrid() const { return _grid; }

    // Get combat system
    [[nodiscard]] CombatSystem& GetCombatSystem() { return _combat; }

    // Turn actions
    bool SelectTerritory(TerritoryId territory);
    bool Attack(TerritoryId target);
    void CancelSelection();
    void EndTurn();

    // Update (call each frame)
    void Update(float deltaTime);

    // Check if attack is valid
    [[nodiscard]] bool CanAttack(TerritoryId from, TerritoryId to) const;

    // Get valid attack targets from selected territory
    [[nodiscard]] std::vector<TerritoryId> GetValidTargets(TerritoryId from) const;

private:
    GameState _state;
    HexGrid _grid;
    CombatSystem _combat;
    TerritoryGenerator _generator;
    AIController* _aiController = nullptr;

    // AI timing
    float _aiThinkTimer = 0.0f;
    static constexpr float AI_THINK_DELAY = 0.5f;  // Delay between AI actions

    // Turn flow
    void StartTurn(PlayerId player);
    void AdvanceToNextPlayer();

    // Reinforcement
    int CalculateReinforcements(PlayerId player);
    void DistributeReinforcements(PlayerId player, int diceCount);
    int FindLargestContiguousRegion(PlayerId player);

    // Victory
    void CheckVictory();
    void CheckElimination();

    // Update valid targets based on selection
    void UpdateValidTargets();
};

#endif // ATLAS_GAMECONTROLLER_H

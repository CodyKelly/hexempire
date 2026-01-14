//
// GameController.h - Game flow and turn management
//

#ifndef ATLAS_GAMECONTROLLER_H
#define ATLAS_GAMECONTROLLER_H

#include "GameData.h"
#include "CombatSystem.h"
#include "CombatQueue.h"
#include "../hex/HexGrid.h"
#include "../hex/TerritoryGenerator.h"

class AIController;  // Forward declaration
class ReplaySystem;  // Forward declaration

class GameController
{
public:
    GameController();

    // Initialize a new game
    void InitializeGame(const GameConfig& config);

    // Set AI controller reference
    void SetAIController(AIController* ai) { _aiController = ai; }

    // Set replay system reference
    void SetReplaySystem(ReplaySystem* replay) { _replaySystem = replay; }

    // Get game state
    [[nodiscard]] GameState& GetState() { return _state; }
    [[nodiscard]] const GameState& GetState() const { return _state; }

    // Get hex grid
    [[nodiscard]] HexGrid& GetGrid() { return _grid; }
    [[nodiscard]] const HexGrid& GetGrid() const { return _grid; }

    // Get combat system
    [[nodiscard]] CombatSystem& GetCombatSystem() { return _combat; }

    // Get combat queue
    [[nodiscard]] CombatQueue& GetCombatQueue() { return _combatQueue; }
    [[nodiscard]] const CombatQueue& GetCombatQueue() const { return _combatQueue; }

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

    // Get size of largest contiguous territory region for a player (for UI)
    [[nodiscard]] int FindLargestContiguousRegion(PlayerId player);

private:
    GameState _state;
    HexGrid _grid;
    CombatSystem _combat;
    CombatQueue _combatQueue;
    TerritoryGenerator _generator;
    AIController* _aiController = nullptr;
    ReplaySystem* _replaySystem = nullptr;

    // AI timing
    float _aiThinkTimer = 0.0f;
    static constexpr float AI_THINK_DELAY = 0.1f;  // Delay between AI actions

    // Turn flow
    void StartTurn(PlayerId player);
    void AdvanceToNextPlayer();

    // Reinforcement
    int CalculateReinforcements(PlayerId player);
    void DistributeReinforcements(PlayerId player, int diceCount);

    // Victory
    void CheckVictory();
    void CheckElimination();

    // Update valid targets based on selection
    void UpdateValidTargets();

    // Combat queue processing
    void ProcessCombatQueue();
    void ExecuteCombat(const CombatAction& action);
};

#endif // ATLAS_GAMECONTROLLER_H

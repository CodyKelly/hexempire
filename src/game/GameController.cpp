//
// GameController.cpp - Game flow implementation
//

#include "GameController.h"
#include "AIController.h"
#include <queue>
#include <unordered_set>
#include <algorithm>

#include <tracy/Tracy.hpp>

GameController::GameController()
    : _grid(HexGridConfig{}),
      _generator(0) {
}

void GameController::InitializeGame(const GameConfig &config) {
    ZoneScoped;
    _state = GameState{};
    _state.config = config;

    // Initialize hex grid
    HexGridConfig gridConfig;
    gridConfig.width = config.gridWidth;
    gridConfig.height = config.gridHeight;
    gridConfig.hexSize = config.hexSize;
    gridConfig.noiseSeed = config.seed;
    _grid = HexGrid(gridConfig);

    // Initialize RNG with seed
    _generator = TerritoryGenerator(config.seed);
    _combat = CombatSystem(config.seed);

    // Generate territories
    _generator.Generate(_grid, _state);

    // Initialize players
    _state.activePlayerCount = config.playerCount;
    for (int i = 0; i < config.playerCount; i++) {
        PlayerData &player = _state.players[i];
        player.id = static_cast<PlayerId>(i);
        player.isHuman = (i == config.humanPlayerIndex);
        player.isEliminated = false;
        player.SetColorFromPalette();
        player.name = player.isHuman ? "Player" : "AI " + std::to_string(i);
    }

    // Assign territories to players
    _generator.AssignToPlayers(_state);

    // Start game with first player
    _state.turnNumber = 1;
    StartTurn(0);
}

void GameController::StartTurn(PlayerId player) {
    _state.currentPlayer = player;
    _state.selectedTerritory = TERRITORY_NONE;
    _state.validTargets.clear();

    if (_state.players[player].isHuman) {
        _state.phase = TurnPhase::SelectAttacker;
    } else {
        _state.phase = TurnPhase::AITurn;
        _aiThinkTimer = AI_THINK_DELAY;
    }
}

void GameController::AdvanceToNextPlayer() {
    // Find next non-eliminated player
    PlayerId next = _state.currentPlayer;
    do {
        next = (next + 1) % _state.config.playerCount;

        // Check if we've gone full circle
        if (next == _state.currentPlayer) {
            // No other players - game over
            _state.phase = TurnPhase::GameOver;
            _state.winner = _state.currentPlayer;
            return;
        }
    } while (_state.players[next].isEliminated);

    // Increment turn number when wrapping back to first player
    if (next <= _state.currentPlayer) {
        _state.turnNumber++;
    }

    StartTurn(next);
}

bool GameController::SelectTerritory(TerritoryId territory) {
    if (_state.phase != TurnPhase::SelectAttacker &&
        _state.phase != TurnPhase::SelectTarget) {
        return false;
    }

    if (territory == _state.selectedTerritory) {
        CancelSelection();
        return false;
    }

    const TerritoryData *t = _state.GetTerritory(territory);
    if (!t) return false;

    // Check if this is our territory
    if (t->owner != _state.currentPlayer) return false;

    // Check if it can attack
    if (!t->CanAttack()) return false;

    _state.selectedTerritory = territory;
    _state.phase = TurnPhase::SelectTarget;
    UpdateValidTargets();

    return true;
}

bool GameController::Attack(TerritoryId target) {
    ZoneScoped;
    if (_state.phase != TurnPhase::SelectTarget &&
        _state.phase != TurnPhase::AITurn) {
        return false;
    }

    TerritoryId from = _state.selectedTerritory;
    if (from == TERRITORY_NONE) return false;

    if (!CanAttack(from, target)) return false;

    const TerritoryData *attacker = _state.GetTerritory(from);
    const TerritoryData *defender = _state.GetTerritory(target);
    if (!attacker || !defender) return false;

    // Resolve combat
    CombatResult result = _combat.ResolveCombat(*attacker, *defender);
    _combat.ApplyCombatResult(_state, result);

    // Store result for display
    _state.lastCombat = result;
    _state.combatPending = true;
    _state.combatAnimTimer = 1.5f; // Show result for 1.5 seconds

    // Check for elimination and victory
    CheckElimination();
    CheckVictory();

    // Reset selection
    _state.selectedTerritory = TERRITORY_NONE;
    _state.validTargets.clear();

    if (!_state.IsGameOver()) {
        if (_state.players[_state.currentPlayer].isHuman) {
            _state.phase = TurnPhase::SelectAttacker;
        }
    }

    return true;
}

void GameController::CancelSelection() {
    _state.selectedTerritory = TERRITORY_NONE;
    _state.validTargets.clear();
    _state.phase = TurnPhase::SelectAttacker;
}

void GameController::EndTurn() {
    ZoneScoped;
    if (_state.phase == TurnPhase::GameOver) return;

    CancelSelection();

    // Calculate and distribute reinforcements
    int reinforcements = CalculateReinforcements(_state.currentPlayer);
    if (reinforcements > 0) {
        DistributeReinforcements(_state.currentPlayer, reinforcements);
    }

    // Move to next player
    AdvanceToNextPlayer();
}

bool GameController::CanAttack(TerritoryId from, TerritoryId to) const {
    const TerritoryData *attacker = _state.GetTerritory(from);
    const TerritoryData *defender = _state.GetTerritory(to);

    if (!attacker || !defender) return false;

    // Must be our territory
    if (attacker->owner != _state.currentPlayer) return false;

    // Must have at least 2 dice
    if (!attacker->CanAttack()) return false;

    // Must be enemy territory
    if (defender->owner == _state.currentPlayer) return false;

    // Must be neighbors
    auto &neighbors = attacker->neighbors;
    return std::find(neighbors.begin(), neighbors.end(), to) != neighbors.end();
}

std::vector<TerritoryId> GameController::GetValidTargets(TerritoryId from) const {
    std::vector<TerritoryId> targets;

    const TerritoryData *attacker = _state.GetTerritory(from);
    if (!attacker || !attacker->CanAttack()) return targets;

    for (TerritoryId neighbor: attacker->neighbors) {
        if (CanAttack(from, neighbor)) {
            targets.push_back(neighbor);
        }
    }

    return targets;
}

void GameController::UpdateValidTargets() {
    _state.validTargets = GetValidTargets(_state.selectedTerritory);
}

void GameController::Update(float deltaTime) {
    ZoneScoped;
    // Handle combat animation timer
    if (_state.combatPending) {
        _state.combatAnimTimer -= deltaTime;
        if (_state.combatAnimTimer <= 0) {
            _state.combatPending = false;
        }
    }

    // Handle AI turn
    if (_state.phase == TurnPhase::AITurn && _aiController) {
        _aiThinkTimer -= deltaTime;
        if (_aiThinkTimer <= 0) {
            // Let AI take an action
            bool tookAction = _aiController->TakeAction(_state.currentPlayer);

            if (tookAction) {
                // Reset timer for next action
                _aiThinkTimer = AI_THINK_DELAY;
            } else {
                // AI is done, end turn
                EndTurn();
            }
        }
    }
}

int GameController::CalculateReinforcements(PlayerId player) {
    return FindLargestContiguousRegion(player);
}

void GameController::DistributeReinforcements(PlayerId player, int diceCount) {
    // Get all territories owned by player that aren't full
    std::vector<TerritoryId> eligibleTerritories;
    for (const auto &t: _state.territories) {
        if (t.owner == player && t.diceCount < MAX_DICE_PER_TERRITORY) {
            eligibleTerritories.push_back(t.id);
        }
    }

    if (eligibleTerritories.empty()) return;

    // Distribute dice randomly
    std::mt19937 rng(std::random_device{}());
    while (diceCount > 0 && !eligibleTerritories.empty()) {
        int idx = std::uniform_int_distribution<>(
            0, static_cast<int>(eligibleTerritories.size()) - 1)(rng);

        TerritoryData *t = _state.GetTerritory(eligibleTerritories[idx]);
        if (t && t->diceCount < MAX_DICE_PER_TERRITORY) {
            t->diceCount++;
            diceCount--;

            // Remove if now full
            if (t->diceCount >= MAX_DICE_PER_TERRITORY) {
                eligibleTerritories.erase(eligibleTerritories.begin() + idx);
            }
        } else {
            eligibleTerritories.erase(eligibleTerritories.begin() + idx);
        }
    }
}

int GameController::FindLargestContiguousRegion(PlayerId player) {
    ZoneScoped;
    // Get all territories owned by player
    std::vector<TerritoryId> playerTerritories;
    for (const auto &t: _state.territories) {
        if (t.owner == player) {
            playerTerritories.push_back(t.id);
        }
    }

    if (playerTerritories.empty()) return 0;

    std::unordered_set<TerritoryId> visited;
    int largestRegion = 0;

    // BFS from each unvisited territory
    for (TerritoryId start: playerTerritories) {
        if (visited.find(start) != visited.end()) continue;

        // BFS to find connected region
        std::queue<TerritoryId> queue;
        queue.push(start);
        visited.insert(start);
        int regionSize = 0;

        while (!queue.empty()) {
            TerritoryId current = queue.front();
            queue.pop();
            regionSize++;

            const TerritoryData *t = _state.GetTerritory(current);
            if (!t) continue;

            for (TerritoryId neighbor: t->neighbors) {
                const TerritoryData *nt = _state.GetTerritory(neighbor);
                if (nt && nt->owner == player &&
                    visited.find(neighbor) == visited.end()) {
                    visited.insert(neighbor);
                    queue.push(neighbor);
                }
            }
        }

        largestRegion = std::max(largestRegion, regionSize);
    }

    return largestRegion;
}

void GameController::CheckVictory() {
    // Check if one player owns all territories
    PlayerId firstOwner = PLAYER_NONE;

    for (const auto &t: _state.territories) {
        if (firstOwner == PLAYER_NONE) {
            firstOwner = t.owner;
        } else if (t.owner != firstOwner) {
            return; // Multiple owners, no victory
        }
    }

    if (firstOwner != PLAYER_NONE) {
        _state.winner = firstOwner;
        _state.phase = TurnPhase::GameOver;
    }
}

void GameController::CheckElimination() {
    // Check each player
    for (int p = 0; p < _state.config.playerCount; p++) {
        if (_state.players[p].isEliminated) continue;

        int territories = _state.CountTerritoriesOwned(static_cast<PlayerId>(p));
        if (territories == 0) {
            _state.players[p].isEliminated = true;
            _state.activePlayerCount--;
        }
    }
}

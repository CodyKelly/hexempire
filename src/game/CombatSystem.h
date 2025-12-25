//
// CombatSystem.h - Dice combat resolution
//

#ifndef ATLAS_COMBATSYSTEM_H
#define ATLAS_COMBATSYSTEM_H

#include "GameData.h"
#include <random>

class CombatSystem
{
public:
    explicit CombatSystem(unsigned int seed = 0);

    // Resolve combat between two territories
    // Returns the combat result with all dice rolls
    CombatResult ResolveCombat(
        const TerritoryData& attacker,
        const TerritoryData& defender
    );

    // Apply combat result to game state
    void ApplyCombatResult(GameState& state, const CombatResult& result);

    // Calculate win probability for attacker (for AI)
    [[nodiscard]] float CalculateWinProbability(int attackerDice, int defenderDice) const;

private:
    std::mt19937 _rng;

    // Roll a single die (1-6)
    int RollDie();

    // Roll multiple dice and return individual results
    std::vector<int> RollDice(int count);
};

#endif // ATLAS_COMBATSYSTEM_H

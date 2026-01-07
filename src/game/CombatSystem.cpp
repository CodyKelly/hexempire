//
// CombatSystem.cpp - Dice combat implementation
//

#include "CombatSystem.h"
#include <numeric>

#include <tracy/Tracy.hpp>

CombatSystem::CombatSystem(unsigned int seed)
{
    if (seed == 0)
    {
        std::random_device rd;
        _rng = std::mt19937(rd());
    }
    else
    {
        _rng = std::mt19937(seed);
    }
}

int CombatSystem::RollDie()
{
    return std::uniform_int_distribution<>(1, 6)(_rng);
}

std::vector<int> CombatSystem::RollDice(int count)
{
    std::vector<int> rolls;
    rolls.reserve(count);
    for (int i = 0; i < count; i++)
    {
        rolls.push_back(RollDie());
    }
    return rolls;
}

CombatResult CombatSystem::ResolveCombat(
    const TerritoryData& attacker,
    const TerritoryData& defender)
{
    ZoneScoped;
    CombatResult result;
    result.attackerId = attacker.id;
    result.defenderId = defender.id;
    result.attackerPlayer = attacker.owner;
    result.defenderPlayer = defender.owner;

    // Roll dice for both sides
    result.attackerRolls = RollDice(attacker.diceCount);
    result.defenderRolls = RollDice(defender.diceCount);

    // Calculate totals
    result.attackerTotal = std::accumulate(
        result.attackerRolls.begin(),
        result.attackerRolls.end(),
        0
    );
    result.defenderTotal = std::accumulate(
        result.defenderRolls.begin(),
        result.defenderRolls.end(),
        0
    );

    // Attacker wins on higher total (ties go to attacker in this variant)
    result.attackerWins = result.attackerTotal > result.defenderTotal;

    return result;
}

void CombatSystem::ApplyCombatResult(GameState& state, const CombatResult& result)
{
    TerritoryData* attacker = state.GetTerritory(result.attackerId);
    TerritoryData* defender = state.GetTerritory(result.defenderId);

    if (!attacker || !defender) return;

    if (result.attackerWins)
    {
        // Attacker captures defender's territory
        // Move all but one die to captured territory
        int movingDice = attacker->diceCount - 1;

        defender->owner = result.attackerPlayer;
        defender->diceCount = static_cast<uint8_t>(movingDice);
        attacker->diceCount = 1;

        // Territory ownership changed - map needs refresh
        state.mapNeedsRefresh = true;
    }
    else
    {
        // Defender wins - attacker loses all but one die
        attacker->diceCount = 1;
    }
}

float CombatSystem::CalculateWinProbability(int attackerDice, int defenderDice) const
{
    // Approximate win probability using expected values and standard deviation
    // Mean for n dice: n * 3.5
    // Variance for n dice: n * (35/12) ≈ 2.917
    // Standard deviation: sqrt(n * 2.917)

    if (attackerDice <= 0 || defenderDice <= 0) return 0.0f;

    float attackerMean = attackerDice * 3.5f;
    float defenderMean = defenderDice * 3.5f;

    float attackerVar = attackerDice * 2.917f;
    float defenderVar = defenderDice * 2.917f;

    // Combined variance of difference
    float diffVar = attackerVar + defenderVar;
    float diffStd = std::sqrt(diffVar);

    // Difference in means
    float diffMean = attackerMean - defenderMean;

    // Approximate probability using normal distribution
    // P(attacker > defender) ≈ P(Z > -diffMean/diffStd)
    // Using error function approximation
    float z = diffMean / diffStd;

    // Approximate CDF using logistic function (good approximation to normal CDF)
    float prob = 1.0f / (1.0f + std::exp(-1.7f * z));

    return prob;
}

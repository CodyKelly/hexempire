//
// AIController.h - Strategic AI for computer players
//

#ifndef ATLAS_AICONTROLLER_H
#define ATLAS_AICONTROLLER_H

#include "GameData.h"
#include "CombatSystem.h"
#include <random>
#include <unordered_set>

class GameController;

// Represents a contiguous region of territories owned by a player
struct ContiguousRegion
{
    std::unordered_set<TerritoryId> territories;
    int totalDice = 0;

    [[nodiscard]] int Size() const { return static_cast<int>(territories.size()); }
    [[nodiscard]] bool Contains(TerritoryId id) const { return territories.count(id) > 0; }
};

// Evaluation of a potential attack
struct AttackEvaluation
{
    TerritoryId from;
    TerritoryId to;
    float score;                // Overall score (higher = better)
    float winProbability;       // Estimated chance of winning
    int attackerDice;
    int defenderDice;

    // Additional metadata for decision-making
    bool fromLargestRegion = false;
    bool wouldConnect = false;
    int potentialIncomeGain = 0;  // If this connects regions, how much income would we gain
};

class AIController
{
public:
    AIController(GameController* controller, unsigned int seed = 0);

    // Take a single action (attack or pass)
    // Returns true if an attack was made, false if done attacking
    bool TakeAction(PlayerId player);

private:
    GameController* _controller;
    CombatSystem* _combat;
    std::mt19937 _rng;

    // Tuning parameters
    static constexpr float MIN_WIN_PROBABILITY = 0.40f;  // Don't attack if below this
    static constexpr float MIN_ATTACK_SCORE = 0.3f;      // Minimum score to consider attack

    // Strategic weights
    static constexpr float WEIGHT_RETRIBUTION = 0.4f;    // How much to prioritize revenge
    static constexpr float WEIGHT_HONOR = 0.2f;          // How much to avoid attacking peaceful players
    static constexpr float WEIGHT_CONNECTION = 1.5f;     // Bonus for connecting regions
    static constexpr float WEIGHT_INCOME_GAIN = 0.15f;   // Per-territory income gain bonus
    static constexpr float WEIGHT_EXPOSURE_RISK = 0.3f;  // Penalty for exposing to strong enemies
    static constexpr float WEIGHT_NON_MAIN_REGION = 0.5f; // Penalty multiplier for attacking from non-main region

    // Evaluate all possible attacks for player
    std::vector<AttackEvaluation> EvaluateAttacks(PlayerId player);

    // Score a potential attack
    float ScoreAttack(const AttackEvaluation& eval, PlayerId player,
                     const std::vector<ContiguousRegion>& regions,
                     const ContiguousRegion* largestRegion);

    // Find all contiguous regions for a player
    std::vector<ContiguousRegion> FindContiguousRegions(PlayerId player);

    // Find the largest region (returns pointer to region in vector, or nullptr)
    const ContiguousRegion* FindLargestRegion(const std::vector<ContiguousRegion>& regions);

    // Check if attack would connect separated regions and calculate income gain
    bool WouldConnectRegions(TerritoryId target, PlayerId player,
                            const std::vector<ContiguousRegion>& regions,
                            int* outIncomeGain = nullptr);

    // Calculate exposure risk - how dangerous would our position be after attacking
    float CalculateExposureRisk(TerritoryId from, TerritoryId to, PlayerId player);

    // Calculate retribution score - bonus for attacking players who attacked us
    float CalculateRetributionScore(PlayerId defender, PlayerId attacker);

    // Calculate honor penalty - penalty for attacking players who've left us alone
    float CalculateHonorPenalty(PlayerId defender, PlayerId attacker);

    // Count how many enemy neighbors a territory has
    int CountEnemyNeighbors(TerritoryId territory, PlayerId player);

    // Count how many friendly neighbors a territory has
    int CountFriendlyNeighbors(TerritoryId territory, PlayerId player);

    // Get the strongest enemy dice adjacent to a territory
    int GetStrongestAdjacentEnemy(TerritoryId territory, PlayerId player);

    // Evaluate strategic value of a territory
    float EvaluateTerritoryValue(TerritoryId territory, PlayerId player);
};

#endif // ATLAS_AICONTROLLER_H

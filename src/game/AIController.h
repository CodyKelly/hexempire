//
// AIController.h - Strategic AI for computer players
//

#ifndef ATLAS_AICONTROLLER_H
#define ATLAS_AICONTROLLER_H

#include "GameData.h"
#include "CombatSystem.h"
#include <random>

class GameController;

// Evaluation of a potential attack
struct AttackEvaluation
{
    TerritoryId from;
    TerritoryId to;
    float score;                // Overall score (higher = better)
    float winProbability;       // Estimated chance of winning
    int attackerDice;
    int defenderDice;
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

    // Evaluate all possible attacks for player
    std::vector<AttackEvaluation> EvaluateAttacks(PlayerId player);

    // Score a potential attack
    float ScoreAttack(const AttackEvaluation& eval, PlayerId player);

    // Check if attack would connect separated regions
    bool WouldConnectRegions(TerritoryId target, PlayerId player);

    // Count how many enemy neighbors a territory has
    int CountEnemyNeighbors(TerritoryId territory, PlayerId player);

    // Count how many friendly neighbors a territory has
    int CountFriendlyNeighbors(TerritoryId territory, PlayerId player);

    // Evaluate strategic value of a territory
    float EvaluateTerritoryValue(TerritoryId territory, PlayerId player);
};

#endif // ATLAS_AICONTROLLER_H

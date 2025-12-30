//
// AIController.cpp - Strategic AI implementation
//

#include "AIController.h"
#include "GameController.h"
#include <algorithm>
#include <queue>
#include <unordered_set>

#include <tracy/Tracy.hpp>

AIController::AIController(GameController* controller, unsigned int seed)
    : _controller(controller),
      _combat(&controller->GetCombatSystem())
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

bool AIController::TakeAction(PlayerId player)
{
    ZoneScoped;
    // Evaluate all possible attacks
    std::vector<AttackEvaluation> attacks = EvaluateAttacks(player);

    if (attacks.empty())
    {
        return false;  // No attacks available
    }

    // Sort by score (highest first)
    std::sort(attacks.begin(), attacks.end(),
        [](const AttackEvaluation& a, const AttackEvaluation& b) {
            return a.score > b.score;
        });

    // Get the best attack
    const AttackEvaluation& best = attacks[0];

    // Check if it meets minimum thresholds
    if (best.winProbability < MIN_WIN_PROBABILITY ||
        best.score < MIN_ATTACK_SCORE)
    {
        return false;  // Best attack isn't good enough
    }

    // Add some randomness - occasionally pick second best if close
    size_t choiceIdx = 0;
    if (attacks.size() > 1 && attacks[1].score > best.score * 0.9f)
    {
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(_rng) < 0.3f)
        {
            choiceIdx = 1;
        }
    }

    const AttackEvaluation& chosen = attacks[choiceIdx];

    // Execute the attack
    GameState& state = _controller->GetState();
    state.selectedTerritory = chosen.from;
    return _controller->Attack(chosen.to);
}

std::vector<AttackEvaluation> AIController::EvaluateAttacks(PlayerId player)
{
    ZoneScoped;
    std::vector<AttackEvaluation> evaluations;
    const GameState& state = _controller->GetState();

    for (const TerritoryData& territory : state.territories)
    {
        // Skip if not ours or can't attack
        if (territory.owner != player || !territory.CanAttack())
        {
            continue;
        }

        // Check each neighbor
        for (TerritoryId neighborId : territory.neighbors)
        {
            const TerritoryData* neighbor = state.GetTerritory(neighborId);
            if (!neighbor || neighbor->owner == player)
            {
                continue;
            }

            AttackEvaluation eval;
            eval.from = territory.id;
            eval.to = neighborId;
            eval.attackerDice = territory.diceCount;
            eval.defenderDice = neighbor->diceCount;
            eval.winProbability = _combat->CalculateWinProbability(
                eval.attackerDice, eval.defenderDice);
            eval.score = ScoreAttack(eval, player);

            evaluations.push_back(eval);
        }
    }

    return evaluations;
}

float AIController::ScoreAttack(const AttackEvaluation& eval, PlayerId player)
{
    ZoneScoped;
    const GameState& state = _controller->GetState();
    float score = eval.winProbability;

    // Bonus for attacking weaker targets
    if (eval.defenderDice < eval.attackerDice)
    {
        score *= 1.2f;
    }

    // Bonus for overwhelming force (high win probability)
    if (eval.winProbability > 0.7f)
    {
        score *= 1.3f;
    }

    // Bonus for connecting regions
    if (WouldConnectRegions(eval.to, player))
    {
        score *= 1.5f;
    }

    // Bonus for attacking territories with many enemy neighbors
    // (reduces enemy's options)
    int enemyNeighbors = CountEnemyNeighbors(eval.to, state.GetTerritory(eval.to)->owner);
    if (enemyNeighbors >= 3)
    {
        score *= 1.2f;
    }

    // Penalty for leaving ourselves exposed (few dice remaining)
    if (eval.attackerDice == 2)
    {
        score *= 0.8f;  // Only 1 die left after attack
    }

    // Bonus for high value targets
    float targetValue = EvaluateTerritoryValue(eval.to, player);
    score *= (1.0f + targetValue * 0.3f);

    // Penalty for attacking strong defenders
    if (eval.defenderDice >= 6)
    {
        score *= 0.7f;
    }

    return score;
}

bool AIController::WouldConnectRegions(TerritoryId target, PlayerId player)
{
    ZoneScoped;
    const GameState& state = _controller->GetState();
    const TerritoryData* territory = state.GetTerritory(target);
    if (!territory) return false;

    // Count how many distinct friendly regions this territory touches
    std::unordered_set<TerritoryId> friendlyNeighborRegions;

    for (TerritoryId neighborId : territory->neighbors)
    {
        const TerritoryData* neighbor = state.GetTerritory(neighborId);
        if (neighbor && neighbor->owner == player)
        {
            // Do BFS to find region ID (use lowest territory ID in region)
            std::queue<TerritoryId> queue;
            std::unordered_set<TerritoryId> visited;
            TerritoryId regionId = neighborId;

            queue.push(neighborId);
            visited.insert(neighborId);

            while (!queue.empty())
            {
                TerritoryId current = queue.front();
                queue.pop();
                regionId = std::min(regionId, current);

                const TerritoryData* t = state.GetTerritory(current);
                if (!t) continue;

                for (TerritoryId n : t->neighbors)
                {
                    const TerritoryData* nt = state.GetTerritory(n);
                    if (nt && nt->owner == player &&
                        visited.find(n) == visited.end())
                    {
                        visited.insert(n);
                        queue.push(n);
                    }
                }
            }

            friendlyNeighborRegions.insert(regionId);
        }
    }

    // If we touch more than one region, capturing this would connect them
    return friendlyNeighborRegions.size() > 1;
}

int AIController::CountEnemyNeighbors(TerritoryId territory, PlayerId player)
{
    const GameState& state = _controller->GetState();
    const TerritoryData* t = state.GetTerritory(territory);
    if (!t) return 0;

    int count = 0;
    for (TerritoryId neighborId : t->neighbors)
    {
        const TerritoryData* neighbor = state.GetTerritory(neighborId);
        if (neighbor && neighbor->owner != player)
        {
            count++;
        }
    }
    return count;
}

int AIController::CountFriendlyNeighbors(TerritoryId territory, PlayerId player)
{
    const GameState& state = _controller->GetState();
    const TerritoryData* t = state.GetTerritory(territory);
    if (!t) return 0;

    int count = 0;
    for (TerritoryId neighborId : t->neighbors)
    {
        const TerritoryData* neighbor = state.GetTerritory(neighborId);
        if (neighbor && neighbor->owner == player)
        {
            count++;
        }
    }
    return count;
}

float AIController::EvaluateTerritoryValue(TerritoryId territory, PlayerId player)
{
    const GameState& state = _controller->GetState();
    const TerritoryData* t = state.GetTerritory(territory);
    if (!t) return 0.0f;

    float value = 0.0f;

    // More neighbors = more strategic value
    value += t->neighbors.size() * 0.1f;

    // Fewer enemy neighbors = safer position after capture
    int enemyNeighbors = CountEnemyNeighbors(territory, player);
    int friendlyAfterCapture = CountFriendlyNeighbors(territory, player);

    if (friendlyAfterCapture > enemyNeighbors)
    {
        value += 0.3f;  // Defensible position
    }

    // Territories with more hexes are slightly more valuable (larger visual presence)
    value += t->hexes.size() * 0.02f;

    return std::min(value, 1.0f);  // Cap at 1.0
}

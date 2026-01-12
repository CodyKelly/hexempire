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

    // Pre-compute contiguous regions for strategic evaluation
    std::vector<ContiguousRegion> regions = FindContiguousRegions(player);
    const ContiguousRegion* largestRegion = FindLargestRegion(regions);

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

            // Check if attacking from largest region
            eval.fromLargestRegion = largestRegion && largestRegion->Contains(territory.id);

            // Check if this would connect regions
            int incomeGain = 0;
            eval.wouldConnect = WouldConnectRegions(neighborId, player, regions, &incomeGain);
            eval.potentialIncomeGain = incomeGain;

            eval.score = ScoreAttack(eval, player, regions, largestRegion);

            evaluations.push_back(eval);
        }
    }

    return evaluations;
}

float AIController::ScoreAttack(const AttackEvaluation& eval, PlayerId player,
                               [[maybe_unused]] const std::vector<ContiguousRegion>& regions,
                               const ContiguousRegion* largestRegion)
{
    ZoneScoped;
    const GameState& state = _controller->GetState();
    const TerritoryData* target = state.GetTerritory(eval.to);
    if (!target) return 0.0f;

    float score = eval.winProbability;

    // === BASE COMBAT MODIFIERS ===

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

    // Penalty for leaving ourselves exposed (few dice remaining)
    if (eval.attackerDice == 2)
    {
        score *= 0.8f;  // Only 1 die left after attack
    }

    // Penalty for attacking strong defenders
    if (eval.defenderDice >= 6)
    {
        score *= 0.7f;
    }

    // === STRATEGIC REGION-BASED MODIFIERS ===

    // Major bonus for connecting regions - this is a key strategic move
    if (eval.wouldConnect)
    {
        score *= WEIGHT_CONNECTION;

        // Additional bonus based on how much income we'd gain
        // This makes "moonshot" connections very attractive
        if (eval.potentialIncomeGain > 0)
        {
            score *= (1.0f + WEIGHT_INCOME_GAIN * eval.potentialIncomeGain);
        }
    }

    // Penalty for attacking from non-main region (unless it would connect)
    // Expanding isolated regions just spreads our dice thin without income benefit
    if (!eval.fromLargestRegion && !eval.wouldConnect && largestRegion != nullptr)
    {
        score *= WEIGHT_NON_MAIN_REGION;
    }

    // === EXPOSURE RISK ===

    // Consider what enemies we'd be exposed to after the attack
    float exposureRisk = CalculateExposureRisk(eval.from, eval.to, player);
    score *= (1.0f - WEIGHT_EXPOSURE_RISK * exposureRisk);

    // === RETRIBUTION AND HONOR ===

    PlayerId targetOwner = target->owner;

    // Bonus for attacking players who have attacked us (revenge)
    float retributionBonus = CalculateRetributionScore(targetOwner, player);
    score *= (1.0f + WEIGHT_RETRIBUTION * retributionBonus);

    // Penalty for attacking players who have left us alone (honor)
    float honorPenalty = CalculateHonorPenalty(targetOwner, player);
    score *= (1.0f - WEIGHT_HONOR * honorPenalty);

    // === OTHER STRATEGIC FACTORS ===

    // Bonus for attacking territories with many enemy neighbors
    // (reduces enemy's options)
    int enemyNeighbors = CountEnemyNeighbors(eval.to, target->owner);
    if (enemyNeighbors >= 3)
    {
        score *= 1.2f;
    }

    // Bonus for high value targets
    float targetValue = EvaluateTerritoryValue(eval.to, player);
    score *= (1.0f + targetValue * 0.3f);

    return score;
}

std::vector<ContiguousRegion> AIController::FindContiguousRegions(PlayerId player)
{
    ZoneScoped;
    std::vector<ContiguousRegion> regions;
    const GameState& state = _controller->GetState();

    // Get all territories owned by player
    std::unordered_set<TerritoryId> unvisited;
    for (const auto& t : state.territories)
    {
        if (t.owner == player)
        {
            unvisited.insert(t.id);
        }
    }

    // BFS from each unvisited territory to find connected regions
    while (!unvisited.empty())
    {
        TerritoryId start = *unvisited.begin();
        ContiguousRegion region;

        std::queue<TerritoryId> queue;
        queue.push(start);
        unvisited.erase(start);

        while (!queue.empty())
        {
            TerritoryId current = queue.front();
            queue.pop();

            const TerritoryData* t = state.GetTerritory(current);
            if (!t) continue;

            region.territories.insert(current);
            region.totalDice += t->diceCount;

            for (TerritoryId neighbor : t->neighbors)
            {
                if (unvisited.count(neighbor) > 0)
                {
                    const TerritoryData* nt = state.GetTerritory(neighbor);
                    if (nt && nt->owner == player)
                    {
                        unvisited.erase(neighbor);
                        queue.push(neighbor);
                    }
                }
            }
        }

        regions.push_back(std::move(region));
    }

    return regions;
}

const ContiguousRegion* AIController::FindLargestRegion(const std::vector<ContiguousRegion>& regions)
{
    if (regions.empty()) return nullptr;

    const ContiguousRegion* largest = &regions[0];
    for (const auto& region : regions)
    {
        if (region.Size() > largest->Size())
        {
            largest = &region;
        }
    }
    return largest;
}

bool AIController::WouldConnectRegions(TerritoryId target, PlayerId player,
                                       const std::vector<ContiguousRegion>& regions,
                                       int* outIncomeGain)
{
    ZoneScoped;
    const GameState& state = _controller->GetState();
    const TerritoryData* territory = state.GetTerritory(target);
    if (!territory) return false;

    // Find which regions this territory touches
    std::vector<const ContiguousRegion*> touchedRegions;

    for (TerritoryId neighborId : territory->neighbors)
    {
        const TerritoryData* neighbor = state.GetTerritory(neighborId);
        if (neighbor && neighbor->owner == player)
        {
            // Find which region this neighbor belongs to
            for (const auto& region : regions)
            {
                if (region.Contains(neighborId))
                {
                    // Check if we already have this region
                    bool alreadyTouched = false;
                    for (const auto* touched : touchedRegions)
                    {
                        if (touched == &region)
                        {
                            alreadyTouched = true;
                            break;
                        }
                    }
                    if (!alreadyTouched)
                    {
                        touchedRegions.push_back(&region);
                    }
                    break;
                }
            }
        }
    }

    // If we touch more than one region, capturing this would connect them
    if (touchedRegions.size() > 1)
    {
        if (outIncomeGain)
        {
            // Calculate income gain: sum of all touched region sizes
            // minus the size of the largest one (which we'd already get)
            // plus 1 for the new territory
            int totalSize = 1;  // The captured territory
            int largestTouched = 0;
            for (const auto* region : touchedRegions)
            {
                totalSize += region->Size();
                largestTouched = std::max(largestTouched, region->Size());
            }
            // Before: income = largestTouched
            // After: income = totalSize
            *outIncomeGain = totalSize - largestTouched;
        }
        return true;
    }

    if (outIncomeGain) *outIncomeGain = 0;
    return false;
}

float AIController::CalculateExposureRisk(TerritoryId from, TerritoryId to, PlayerId player)
{
    ZoneScoped;
    const GameState& state = _controller->GetState();
    const TerritoryData* attacker = state.GetTerritory(from);
    const TerritoryData* target = state.GetTerritory(to);
    if (!attacker || !target) return 0.0f;

    // After a successful attack:
    // - Attacker territory has 1 die remaining
    // - Target territory has (attackerDice - 1) dice

    // Risk 1: Our attacking territory becomes vulnerable with 1 die
    int strongestNearAttacker = GetStrongestAdjacentEnemy(from, player);
    float attackerRisk = 0.0f;
    if (strongestNearAttacker >= 3)
    {
        // High risk of losing this territory on counter-attack
        attackerRisk = std::min(1.0f, (strongestNearAttacker - 1) / 7.0f);
    }

    // Risk 2: The captured territory might be surrounded by strong enemies
    // After capture, we'd have (attackerDice - 1) dice there
    int diceAfterCapture = attacker->diceCount - 1;
    float capturedRisk = 0.0f;

    // Check enemies adjacent to target (excluding our attacker territory)
    for (TerritoryId neighborId : target->neighbors)
    {
        if (neighborId == from) continue;  // Skip our attacking territory

        const TerritoryData* neighbor = state.GetTerritory(neighborId);
        if (neighbor && neighbor->owner != player && neighbor->owner != target->owner)
        {
            // This is a different enemy - strong enemies near target are dangerous
            if (neighbor->diceCount >= diceAfterCapture + 2)
            {
                capturedRisk = std::max(capturedRisk,
                    std::min(1.0f, (neighbor->diceCount - diceAfterCapture) / 6.0f));
            }
        }
    }

    // Combined risk (weighted toward attacker vulnerability since that's our established position)
    return attackerRisk * 0.6f + capturedRisk * 0.4f;
}

float AIController::CalculateRetributionScore(PlayerId defender, PlayerId attacker)
{
    const GameState& state = _controller->GetState();

    // Count how many times the defender has attacked us recently
    int attacksAgainstUs = state.attackHistory.CountAttacksFrom(
        defender, attacker, state.turnNumber);

    // Normalize: 1 attack = 0.5 bonus, 2+ attacks = 1.0 bonus
    if (attacksAgainstUs == 0) return 0.0f;
    if (attacksAgainstUs == 1) return 0.5f;
    return 1.0f;
}

float AIController::CalculateHonorPenalty(PlayerId defender, PlayerId attacker)
{
    const GameState& state = _controller->GetState();

    // Check if the defender has been peaceful toward us
    if (state.attackHistory.HasBeenPeaceful(defender, attacker, state.turnNumber))
    {
        // Also check if we've been peaceful toward them
        // If we started the aggression, no honor penalty for continuing
        if (state.attackHistory.HasBeenPeaceful(attacker, defender, state.turnNumber))
        {
            // Both sides peaceful - significant penalty for breaking the peace
            return 1.0f;
        }
        // They're peaceful but we already attacked them - small penalty
        return 0.3f;
    }

    // They've attacked us - no honor penalty
    return 0.0f;
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

int AIController::GetStrongestAdjacentEnemy(TerritoryId territory, PlayerId player)
{
    const GameState& state = _controller->GetState();
    const TerritoryData* t = state.GetTerritory(territory);
    if (!t) return 0;

    int strongest = 0;
    for (TerritoryId neighborId : t->neighbors)
    {
        const TerritoryData* neighbor = state.GetTerritory(neighborId);
        if (neighbor && neighbor->owner != player)
        {
            strongest = std::max(strongest, static_cast<int>(neighbor->diceCount));
        }
    }
    return strongest;
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

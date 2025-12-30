//
// TerritoryGenerator.cpp - Flood-fill territory generation implementation
//

#include "TerritoryGenerator.h"
#include <queue>
#include <algorithm>
#include <unordered_set>

#include <tracy/Tracy.hpp>

TerritoryGenerator::TerritoryGenerator(unsigned int seed)
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

void TerritoryGenerator::Generate(const HexGrid& grid, GameState& state)
{
    ZoneScoped;
    // Clear existing data
    state.territories.clear();
    state.hexToTerritory.clear();

    // Select seed points
    std::vector<HexCoord> seeds = SelectSeedPoints(grid, state.config.targetTerritoryCount);

    // Create territories via flood fill
    FloodFillTerritories(grid, seeds, state);

    // Calculate adjacency
    CalculateTerritoryNeighbors(grid, state);

    // Find center of each territory
    for (auto& territory : state.territories)
    {
        territory.centerHex = FindTerritoryCenter(territory);
    }
}

std::vector<HexCoord> TerritoryGenerator::SelectSeedPoints(
    const HexGrid& grid,
    int targetCount)
{
    const auto& allCoords = grid.GetAllCoords();
    std::vector<HexCoord> seeds;

    if (allCoords.empty() || targetCount <= 0) return seeds;

    // Use a simple approach: shuffle all coords and pick evenly spaced ones
    // with minimum distance enforcement
    std::vector<HexCoord> candidates = allCoords;
    std::shuffle(candidates.begin(), candidates.end(), _rng);

    // Minimum distance between seeds (based on grid size and target count)
    float avgHexesPerTerritory = static_cast<float>(allCoords.size()) / targetCount;
    int minDistance = std::max(1, static_cast<int>(std::sqrt(avgHexesPerTerritory) * 0.8f));

    std::unordered_set<HexCoord, HexCoordHash> usedCoords;

    for (const auto& coord : candidates)
    {
        if (seeds.size() >= static_cast<size_t>(targetCount)) break;

        // Check distance to existing seeds
        bool tooClose = false;
        for (const auto& seed : seeds)
        {
            if (coord.DistanceTo(seed) < minDistance)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
        {
            seeds.push_back(coord);
        }
    }

    // If we couldn't get enough seeds with distance constraint, relax it
    if (seeds.size() < static_cast<size_t>(targetCount) / 2)
    {
        seeds.clear();
        // Just take evenly spaced from shuffled list
        int step = std::max(1, static_cast<int>(candidates.size()) / targetCount);
        for (size_t i = 0; i < candidates.size() && seeds.size() < static_cast<size_t>(targetCount); i += step)
        {
            seeds.push_back(candidates[i]);
        }
    }

    return seeds;
}

void TerritoryGenerator::FloodFillTerritories(
    const HexGrid& grid,
    const std::vector<HexCoord>& seeds,
    GameState& state)
{
    ZoneScoped;
    // Initialize territories from seeds
    for (size_t i = 0; i < seeds.size(); i++)
    {
        TerritoryData territory;
        territory.id = static_cast<TerritoryId>(i);
        territory.owner = PLAYER_NONE;
        territory.diceCount = 1;
        state.territories.push_back(territory);
    }

    // Priority queue: (distance, coord, territoryId)
    using QueueItem = std::tuple<int, HexCoord, TerritoryId>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> pq;

    // Add seeds to queue with distance 0
    for (size_t i = 0; i < seeds.size(); i++)
    {
        pq.push({0, seeds[i], static_cast<TerritoryId>(i)});
    }

    // Track assigned hexes
    std::unordered_set<HexCoord, HexCoordHash> assigned;

    // Flood fill
    while (!pq.empty())
    {
        auto [dist, coord, territoryId] = pq.top();
        pq.pop();

        // Skip if already assigned
        if (assigned.find(coord) != assigned.end()) continue;

        // Assign to territory
        assigned.insert(coord);
        state.territories[territoryId].hexes.push_back(coord);
        state.hexToTerritory[coord] = territoryId;

        // Add unassigned neighbors
        for (const auto& neighbor : grid.GetNeighbors(coord))
        {
            if (assigned.find(neighbor) == assigned.end())
            {
                // Add some randomness to distances to create organic shapes
                int jitter = std::uniform_int_distribution<>(0, 2)(_rng);
                pq.push({dist + 1 + jitter, neighbor, territoryId});
            }
        }
    }
}

void TerritoryGenerator::CalculateTerritoryNeighbors(
    const HexGrid& grid,
    GameState& state)
{
    ZoneScoped;
    // For each territory, find adjacent territories
    for (auto& territory : state.territories)
    {
        std::unordered_set<TerritoryId> neighborSet;

        for (const auto& hex : territory.hexes)
        {
            for (const auto& neighbor : grid.GetNeighbors(hex))
            {
                TerritoryId neighborTerritory = state.GetTerritoryAt(neighbor);
                if (neighborTerritory != TERRITORY_NONE &&
                    neighborTerritory != territory.id)
                {
                    neighborSet.insert(neighborTerritory);
                }
            }
        }

        territory.neighbors.assign(neighborSet.begin(), neighborSet.end());
    }
}

HexCoord TerritoryGenerator::FindTerritoryCenter(const TerritoryData& territory)
{
    if (territory.hexes.empty()) return {0, 0};
    if (territory.hexes.size() == 1) return territory.hexes[0];

    // Calculate centroid in axial coordinates
    float avgQ = 0, avgR = 0;
    for (const auto& hex : territory.hexes)
    {
        avgQ += hex.q;
        avgR += hex.r;
    }
    avgQ /= territory.hexes.size();
    avgR /= territory.hexes.size();

    // Find the hex closest to centroid
    HexCoord center = territory.hexes[0];
    float minDist = std::numeric_limits<float>::max();

    for (const auto& hex : territory.hexes)
    {
        float dx = hex.q - avgQ;
        float dy = hex.r - avgR;
        float dist = dx * dx + dy * dy;
        if (dist < minDist)
        {
            minDist = dist;
            center = hex;
        }
    }

    return center;
}

void TerritoryGenerator::AssignToPlayers(GameState& state)
{
    if (state.territories.empty() || state.config.playerCount <= 0) return;

    // Shuffle territory order for random assignment
    std::vector<TerritoryId> territoryOrder;
    for (size_t i = 0; i < state.territories.size(); i++)
    {
        territoryOrder.push_back(static_cast<TerritoryId>(i));
    }
    std::shuffle(territoryOrder.begin(), territoryOrder.end(), _rng);

    // Assign territories round-robin to players
    for (size_t i = 0; i < territoryOrder.size(); i++)
    {
        PlayerId player = static_cast<PlayerId>(i % state.config.playerCount);
        state.territories[territoryOrder[i]].owner = player;
    }

    // Distribute starting dice to each player
    for (PlayerId p = 0; p < state.config.playerCount; p++)
    {
        // Get all territories owned by this player
        std::vector<TerritoryId> playerTerritories;
        for (const auto& t : state.territories)
        {
            if (t.owner == p) playerTerritories.push_back(t.id);
        }

        if (playerTerritories.empty()) continue;

        // Each territory starts with 1 die, distribute remaining
        int totalStartingDice = state.config.startingDicePerPlayer;
        int diceToDistribute = totalStartingDice - static_cast<int>(playerTerritories.size());

        // Randomly add dice to territories (up to max)
        while (diceToDistribute > 0)
        {
            // Pick random territory
            int idx = std::uniform_int_distribution<>(0, static_cast<int>(playerTerritories.size()) - 1)(_rng);
            TerritoryId tid = playerTerritories[idx];

            if (state.territories[tid].diceCount < MAX_DICE_PER_TERRITORY)
            {
                state.territories[tid].diceCount++;
                diceToDistribute--;
            }
            else
            {
                // This territory is full, remove from candidates
                playerTerritories.erase(playerTerritories.begin() + idx);
                if (playerTerritories.empty()) break;
            }
        }
    }
}

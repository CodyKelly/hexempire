//
// IslandDetector.cpp - Island detection and cleanup implementation
//

#include "IslandDetector.h"
#include <algorithm>
#include <unordered_map>

#include <tracy/Tracy.hpp>

std::vector<Island> IslandDetector::FindIslands(const GameState& state)
{
    ZoneScoped;

    std::vector<Island> islands;
    std::unordered_set<TerritoryId> visited;

    for (const auto& territory : state.territories)
    {
        if (visited.find(territory.id) != visited.end())
        {
            continue;
        }

        // Start new island with DFS
        Island island;
        DFSTerritory(territory.id, state, visited, island);
        islands.push_back(std::move(island));
    }

    return islands;
}

void IslandDetector::DFSTerritory(
    TerritoryId current,
    const GameState& state,
    std::unordered_set<TerritoryId>& visited,
    Island& island)
{
    visited.insert(current);
    island.territories.push_back(current);

    const TerritoryData* territory = state.GetTerritory(current);
    if (territory)
    {
        island.totalHexCount += static_cast<int>(territory->hexes.size());

        // Visit all unvisited neighbors
        for (TerritoryId neighborId : territory->neighbors)
        {
            if (visited.find(neighborId) == visited.end())
            {
                DFSTerritory(neighborId, state, visited, island);
            }
        }
    }
}

std::vector<TerritoryId> IslandDetector::KeepLargestIslandOnly(GameState& state)
{
    ZoneScoped;

    std::vector<Island> islands = FindIslands(state);

    if (islands.size() <= 1)
    {
        return {}; // Nothing to remove
    }

    // Find largest island by hex count
    size_t largestIdx = 0;
    for (size_t i = 1; i < islands.size(); i++)
    {
        if (islands[i].totalHexCount > islands[largestIdx].totalHexCount)
        {
            largestIdx = i;
        }
    }

    // Build set of territories to keep
    std::unordered_set<TerritoryId> keepSet(
        islands[largestIdx].territories.begin(),
        islands[largestIdx].territories.end()
    );

    // Collect removed territory IDs
    std::vector<TerritoryId> removed;
    for (size_t i = 0; i < islands.size(); i++)
    {
        if (i == largestIdx) continue;

        for (TerritoryId tid : islands[i].territories)
        {
            // Remove hexes from hexToTerritory map
            const TerritoryData* territory = state.GetTerritory(tid);
            if (territory)
            {
                for (const auto& hex : territory->hexes)
                {
                    state.hexToTerritory.erase(hex);
                }
            }
            removed.push_back(tid);
        }
    }

    // Rebuild territories vector with only kept territories and remap IDs
    std::vector<TerritoryData> keptTerritories;
    std::unordered_map<TerritoryId, TerritoryId> idRemap; // old -> new

    TerritoryId newId = 0;
    for (auto& territory : state.territories)
    {
        if (keepSet.find(territory.id) != keepSet.end())
        {
            idRemap[territory.id] = newId;
            territory.id = newId;
            keptTerritories.push_back(std::move(territory));
            newId++;
        }
    }

    // Update neighbor references with new IDs
    for (auto& territory : keptTerritories)
    {
        std::vector<TerritoryId> newNeighbors;
        for (TerritoryId neighborId : territory.neighbors)
        {
            auto it = idRemap.find(neighborId);
            if (it != idRemap.end())
            {
                newNeighbors.push_back(it->second);
            }
        }
        territory.neighbors = std::move(newNeighbors);
    }

    // Update hexToTerritory with new IDs
    for (auto& [coord, oldId] : state.hexToTerritory)
    {
        auto it = idRemap.find(oldId);
        if (it != idRemap.end())
        {
            oldId = it->second;
        }
    }

    state.territories = std::move(keptTerritories);
    return removed;
}

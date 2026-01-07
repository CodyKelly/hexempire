//
// IslandDetector.h - Detect and manage connected territory groups (islands)
//

#ifndef ATLAS_ISLANDDETECTOR_H
#define ATLAS_ISLANDDETECTOR_H

#include "../game/GameData.h"
#include <vector>
#include <unordered_set>

// An island is a group of connected territories (territories that can reach each other through neighbors)
struct Island
{
    std::vector<TerritoryId> territories;
    int totalHexCount = 0;  // Sum of all hexes in all territories
};

class IslandDetector
{
public:
    // Find all connected territory groups (islands) in the game state
    static std::vector<Island> FindIslands(const GameState& state);

    // Remove all islands except the largest one
    // Returns the IDs of removed territories (before remapping)
    // WARNING: This modifies state by removing territories and remapping IDs
    static std::vector<TerritoryId> KeepLargestIslandOnly(GameState& state);

private:
    // DFS helper for grouping territories into islands
    static void DFSTerritory(
        TerritoryId current,
        const GameState& state,
        std::unordered_set<TerritoryId>& visited,
        Island& island
    );
};

#endif // ATLAS_ISLANDDETECTOR_H

//
// TerritoryGenerator.h - Flood-fill territory generation
//

#ifndef ATLAS_TERRITORYGENERATOR_H
#define ATLAS_TERRITORYGENERATOR_H

#include "HexGrid.h"
#include "../game/GameData.h"
#include <random>

class TerritoryGenerator
{
public:
    explicit TerritoryGenerator(unsigned int seed = 0);

    // Generate territories and populate game state
    void Generate(const HexGrid& grid, GameState& state);

    // Assign territories to players and distribute initial dice
    void AssignToPlayers(GameState& state);

private:
    std::mt19937 _rng;

    // Select evenly-distributed seed points for territories
    std::vector<HexCoord> SelectSeedPoints(
        const HexGrid& grid,
        int targetCount
    );

    // Flood-fill from seeds to create territories
    void FloodFillTerritories(
        const HexGrid& grid,
        const std::vector<HexCoord>& seeds,
        GameState& state
    );

    // Fill small unassigned hex groups by absorbing them into neighboring territories
    void FillHoles(
        const HexGrid& grid,
        GameState& state,
        int minHoleSize
    );

    // Calculate which territories are adjacent
    void CalculateTerritoryNeighbors(
        const HexGrid& grid,
        GameState& state
    );

    // Find the center hex of a territory (for dice placement)
    HexCoord FindTerritoryCenter(const TerritoryData& territory);
};

#endif // ATLAS_TERRITORYGENERATOR_H

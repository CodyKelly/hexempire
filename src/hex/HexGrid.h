//
// HexGrid.h - Hexagonal grid management and operations
//

#ifndef ATLAS_HEXGRID_H
#define ATLAS_HEXGRID_H

#include "HexCoord.h"
#include <vector>
#include <unordered_set>

struct HexGridConfig {
    int radius = 10; // Grid radius (creates hexagonal-shaped grid)
    float hexSize = 32.0f; // Outer radius of each hex in world units

    // Noise-based landmass generation
    bool useNoiseFilter = true; // Enable Perlin noise filtering for irregular landmass
    float noiseOffsetX = 0.0f; // Noise sample offset X (for map variety)
    float noiseOffsetY = 0.0f; // Noise sample offset Y (for map variety)
    float noiseScale = 0.0025f; // Noise frequency (higher = more detail)
    float noiseCutoff = 0.65f; // Threshold [0-1], higher = less land
    unsigned int noiseSeed = 0; // RNG seed for noise (0 = use default)
};

class HexGrid {
public:
    explicit HexGrid(const HexGridConfig &config);

    // Check if coordinate is within the grid
    [[nodiscard]] bool IsValid(const HexCoord &coord) const;

    // Get all valid neighbors of a coordinate
    [[nodiscard]] std::vector<HexCoord> GetNeighbors(const HexCoord &coord) const;

    // Get all coordinates in the grid
    [[nodiscard]] const std::vector<HexCoord> &GetAllCoords() const { return _coords; }

    // Get total number of hexes
    [[nodiscard]] size_t GetHexCount() const { return _coords.size(); }

    // Coordinate conversions
    [[nodiscard]] Vector2 HexToWorld(const HexCoord &coord) const;

    [[nodiscard]] HexCoord WorldToHex(const Vector2 &worldPos) const;

    // Get configuration
    [[nodiscard]] const HexGridConfig &GetConfig() const { return _config; }
    [[nodiscard]] float GetHexSize() const { return _config.hexSize; }
    [[nodiscard]] int GetRadius() const { return _config.radius; }

    // Get world bounds (for camera)
    [[nodiscard]] Vector2 GetWorldMin() const;

    [[nodiscard]] Vector2 GetWorldMax() const;

    [[nodiscard]] Vector2 GetWorldCenter() const;

private:
    HexGridConfig _config;
    std::vector<HexCoord> _coords;
    std::unordered_set<HexCoord, HexCoordHash> _validCoords;

    void GenerateHexagonalGrid();
};

#endif // ATLAS_HEXGRID_H

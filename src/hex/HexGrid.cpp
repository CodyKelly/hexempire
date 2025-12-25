//
// HexGrid.cpp - Hexagonal grid implementation
//

#include "HexGrid.h"
#include <algorithm>
#include <limits>

HexGrid::HexGrid(const HexGridConfig& config)
    : _config(config)
{
    GenerateHexagonalGrid();
}

void HexGrid::GenerateHexagonalGrid()
{
    _coords.clear();
    _validCoords.clear();

    // Generate hexagonal-shaped grid using cube coordinates constraint
    // A hex is valid if |x| + |y| + |z| <= radius * 2, where x + y + z = 0
    for (int q = -_config.radius; q <= _config.radius; q++)
    {
        int r1 = std::max(-_config.radius, -q - _config.radius);
        int r2 = std::min(_config.radius, -q + _config.radius);
        for (int r = r1; r <= r2; r++)
        {
            HexCoord coord{q, r};
            _coords.push_back(coord);
            _validCoords.insert(coord);
        }
    }
}

bool HexGrid::IsValid(const HexCoord& coord) const
{
    return _validCoords.find(coord) != _validCoords.end();
}

std::vector<HexCoord> HexGrid::GetNeighbors(const HexCoord& coord) const
{
    std::vector<HexCoord> neighbors;
    neighbors.reserve(6);

    for (int i = 0; i < 6; i++)
    {
        HexCoord neighbor = coord.Neighbor(i);
        if (IsValid(neighbor))
        {
            neighbors.push_back(neighbor);
        }
    }

    return neighbors;
}

Vector2 HexGrid::HexToWorld(const HexCoord& coord) const
{
    return HexGeometry::HexToWorld(coord, _config.hexSize);
}

HexCoord HexGrid::WorldToHex(const Vector2& worldPos) const
{
    return HexGeometry::WorldToHex(worldPos, _config.hexSize);
}

Vector2 HexGrid::GetWorldMin() const
{
    if (_coords.empty()) return {0, 0};

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();

    for (const auto& coord : _coords)
    {
        Vector2 world = HexToWorld(coord);
        minX = std::min(minX, world.x);
        minY = std::min(minY, world.y);
    }

    // Subtract hex size to account for hex geometry
    return {minX - _config.hexSize, minY - _config.hexSize};
}

Vector2 HexGrid::GetWorldMax() const
{
    if (_coords.empty()) return {0, 0};

    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (const auto& coord : _coords)
    {
        Vector2 world = HexToWorld(coord);
        maxX = std::max(maxX, world.x);
        maxY = std::max(maxY, world.y);
    }

    // Add hex size to account for hex geometry
    return {maxX + _config.hexSize, maxY + _config.hexSize};
}

Vector2 HexGrid::GetWorldCenter() const
{
    Vector2 min = GetWorldMin();
    Vector2 max = GetWorldMax();
    return {(min.x + max.x) / 2.0f, (min.y + max.y) / 2.0f};
}

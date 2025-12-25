//
// HexCoord.h - Axial coordinate system for hexagonal grids
//

#ifndef ATLAS_HEXCOORD_H
#define ATLAS_HEXCOORD_H

#include "../math.h"
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <array>
#include <functional>

// Axial coordinates for pointy-top hexagonal grids
// Uses (q, r) coordinate system where:
//   q = column (diagonal axis)
//   r = row
struct HexCoord
{
    int q = 0;
    int r = 0;

    HexCoord() = default;
    constexpr HexCoord(int q, int r) : q(q), r(r) {}

    // Cube coordinate conversions (for distance calculations)
    [[nodiscard]] constexpr int CubeX() const { return q; }
    [[nodiscard]] constexpr int CubeY() const { return -q - r; }
    [[nodiscard]] constexpr int CubeZ() const { return r; }

    // Distance between two hex coordinates (in hex steps)
    [[nodiscard]] static int Distance(const HexCoord& a, const HexCoord& b)
    {
        // Manhattan distance in cube coordinates, divided by 2
        return (std::abs(a.CubeX() - b.CubeX()) +
                std::abs(a.CubeY() - b.CubeY()) +
                std::abs(a.CubeZ() - b.CubeZ())) / 2;
    }

    [[nodiscard]] int DistanceTo(const HexCoord& other) const
    {
        return Distance(*this, other);
    }

    // Get neighbor in specified direction (0-5)
    [[nodiscard]] HexCoord Neighbor(int direction) const;

    // Operators
    [[nodiscard]] constexpr bool operator==(const HexCoord& other) const
    {
        return q == other.q && r == other.r;
    }

    [[nodiscard]] constexpr bool operator!=(const HexCoord& other) const
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr HexCoord operator+(const HexCoord& other) const
    {
        return HexCoord(q + other.q, r + other.r);
    }

    [[nodiscard]] constexpr HexCoord operator-(const HexCoord& other) const
    {
        return HexCoord(q - other.q, r - other.r);
    }

    HexCoord& operator+=(const HexCoord& other)
    {
        q += other.q;
        r += other.r;
        return *this;
    }

    HexCoord& operator-=(const HexCoord& other)
    {
        q -= other.q;
        r -= other.r;
        return *this;
    }

    // For ordered containers (std::map, std::set)
    [[nodiscard]] constexpr bool operator<(const HexCoord& other) const
    {
        if (q != other.q) return q < other.q;
        return r < other.r;
    }
};

// 6 neighbor directions for pointy-top hexagons
// Starting from East, going counter-clockwise
inline const HexCoord HEX_DIRECTIONS[6] = {
    HexCoord(+1,  0),  // East
    HexCoord(+1, -1),  // Northeast
    HexCoord( 0, -1),  // Northwest
    HexCoord(-1,  0),  // West
    HexCoord(-1, +1),  // Southwest
    HexCoord( 0, +1)   // Southeast
};

inline HexCoord HexCoord::Neighbor(int direction) const
{
    return *this + HEX_DIRECTIONS[direction % 6];
}

// Hash function for use in unordered_map/unordered_set
struct HexCoordHash
{
    std::size_t operator()(const HexCoord& coord) const noexcept
    {
        // Combine q and r using a simple hash
        std::size_t h1 = std::hash<int>{}(coord.q);
        std::size_t h2 = std::hash<int>{}(coord.r);
        return h1 ^ (h2 << 1);
    }
};

// Hex geometry constants for pointy-top orientation
namespace HexGeometry
{
    // Convert hex coordinate to world position (center of hex)
    // hexSize = distance from center to corner (outer radius)
    inline Vector2 HexToWorld(const HexCoord& coord, float hexSize)
    {
        // Pointy-top hex layout
        // x = size * (sqrt(3) * q + sqrt(3)/2 * r)
        // y = size * (3/2 * r)
        constexpr float SQRT3 = 1.7320508075688772f;
        float x = hexSize * (SQRT3 * coord.q + SQRT3 / 2.0f * coord.r);
        float y = hexSize * (1.5f * coord.r);
        return Vector2(x, y);
    }

    // Convert world position to hex coordinate (rounded to nearest hex)
    inline HexCoord WorldToHex(const Vector2& worldPos, float hexSize)
    {
        // Inverse of HexToWorld, then round to nearest hex
        constexpr float SQRT3 = 1.7320508075688772f;

        // Convert to fractional axial coordinates
        float fq = (SQRT3 / 3.0f * worldPos.x - 1.0f / 3.0f * worldPos.y) / hexSize;
        float fr = (2.0f / 3.0f * worldPos.y) / hexSize;

        // Round to nearest hex using cube coordinates
        float x = fq;
        float z = fr;
        float y = -x - z;

        int rx = static_cast<int>(std::round(x));
        int ry = static_cast<int>(std::round(y));
        int rz = static_cast<int>(std::round(z));

        float xDiff = std::abs(static_cast<float>(rx) - x);
        float yDiff = std::abs(static_cast<float>(ry) - y);
        float zDiff = std::abs(static_cast<float>(rz) - z);

        // Reset the coordinate with the largest rounding error
        if (xDiff > yDiff && xDiff > zDiff)
        {
            rx = -ry - rz;
        }
        else if (yDiff > zDiff)
        {
            ry = -rx - rz;
        }
        else
        {
            rz = -rx - ry;
        }

        return HexCoord(rx, rz);
    }

    // Get the 6 corner vertices of a hex in world space
    inline std::array<Vector2, 6> GetHexCorners(const Vector2& center, float hexSize)
    {
        std::array<Vector2, 6> corners;
        for (int i = 0; i < 6; i++)
        {
            // Pointy-top: first corner at 30 degrees
            float angleDeg = 60.0f * static_cast<float>(i) + 30.0f;
            float angleRad = angleDeg * 3.14159265359f / 180.0f;
            corners[i] = Vector2(
                center.x + hexSize * std::cos(angleRad),
                center.y + hexSize * std::sin(angleRad)
            );
        }
        return corners;
    }

    // Get inner radius (distance from center to edge midpoint)
    inline float InnerRadius(float outerRadius)
    {
        constexpr float SQRT3_OVER_2 = 0.8660254037844386f;
        return outerRadius * SQRT3_OVER_2;
    }
}

#endif // ATLAS_HEXCOORD_H

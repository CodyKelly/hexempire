//
// HexMapData.h - Hex tile data for GPU rendering
//

#ifndef ATLAS_HEXMAPDATA_H
#define ATLAS_HEXMAPDATA_H

#include "HexCoord.h"
#include "HexGrid.h"
#include "../game/GameData.h"
#include <vector>

// GPU-compatible hex tile data (must match shader struct)
// Aligned to 16-byte boundaries for GPU
struct HexTileGPU
{
    // Position in world space
    float posX, posY;
    // Hex size (outer radius)
    float hexSize;
    // Flags: bit 0 = selected, bit 1 = hovered, bit 2 = valid target, bit 3 = border
    uint32_t flags;
    // Territory color (from owner)
    float r, g, b, a;
    // Highlight color overlay
    float highlightR, highlightG, highlightB, highlightA;
};

// Flag bits
constexpr uint32_t HEX_FLAG_SELECTED = 1 << 0;
constexpr uint32_t HEX_FLAG_HOVERED = 1 << 1;
constexpr uint32_t HEX_FLAG_VALID_TARGET = 1 << 2;
constexpr uint32_t HEX_FLAG_BORDER = 1 << 3;

class HexMapData
{
public:
    HexMapData();

    // Initialize tiles from hex grid
    void Initialize(const HexGrid& grid);

    // Update territory colors and borders (call once after territories are generated)
    void UpdateFromTerritories(const HexGrid& grid, const GameState& state);

    // Update tile data from game state (optimized - only updates changed UI highlights)
    void UpdateFromGameState(
        const HexGrid& grid,
        const GameState& state,
        const UIState& ui
    );

    // Mark data as needing upload
    void MarkDirty() { _isDirty = true; }
    [[nodiscard]] bool IsDirty() const { return _isDirty; }
    void ClearDirty() { _isDirty = false; }

    // Access tile data
    [[nodiscard]] std::vector<HexTileGPU>& GetTiles() { return _tiles; }
    [[nodiscard]] const std::vector<HexTileGPU>& GetTiles() const { return _tiles; }
    [[nodiscard]] size_t GetTileCount() const { return _tiles.size(); }

    // Get hex size
    [[nodiscard]] float GetHexSize() const { return _hexSize; }

private:
    std::vector<HexTileGPU> _tiles;
    std::unordered_map<HexCoord, size_t, HexCoordHash> _coordToIndex;
    float _hexSize = 24.0f;
    bool _isDirty = true;

    // Cached UI state to detect changes
    std::vector<HexCoord> _cachedSelectedHexes;
    std::vector<HexCoord> _cachedHoverHexes;
    std::vector<HexCoord> _cachedTargetHexes;

    // Helper to update highlight for a single tile
    void UpdateTileHighlight(HexTileGPU& tile, uint32_t uiFlags);

    // Determine if this hex is on a territory border (uses neighbor directions directly)
    bool IsOnBorder(
        const HexCoord& coord,
        const HexGrid& grid,
        const GameState& state
    ) const;
};

#endif // ATLAS_HEXMAPDATA_H

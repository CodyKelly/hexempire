//
// HexMapData.cpp - Hex tile data implementation
//

#include "HexMapData.h"

#include <tracy/Tracy.hpp>

HexMapData::HexMapData()
    : _isDirty(true)
{
}

void HexMapData::Initialize(const HexGrid& grid)
{
    ZoneScoped;
    _tiles.clear();
    _coordToIndex.clear();
    _hexSize = grid.GetHexSize();

    const auto& coords = grid.GetAllCoords();
    _tiles.reserve(coords.size());

    for (size_t i = 0; i < coords.size(); i++)
    {
        const HexCoord& coord = coords[i];
        Vector2 worldPos = grid.HexToWorld(coord);

        HexTileGPU tile{};
        tile.posX = worldPos.x;
        tile.posY = worldPos.y;
        tile.hexSize = _hexSize;
        tile.flags = 0;
        tile.r = 0.3f;
        tile.g = 0.3f;
        tile.b = 0.3f;
        tile.a = 1.0f;
        tile.highlightR = 0.0f;
        tile.highlightG = 0.0f;
        tile.highlightB = 0.0f;
        tile.highlightA = 0.0f;

        _tiles.push_back(tile);
        _coordToIndex[coord] = i;
    }

    _isDirty = true;
}

void HexMapData::UpdateFromTerritories(const HexGrid& grid, const GameState& state)
{
    ZoneScoped;
    const auto& coords = grid.GetAllCoords();

    for (size_t i = 0; i < coords.size() && i < _tiles.size(); i++)
    {
        const HexCoord& coord = coords[i];
        HexTileGPU& tile = _tiles[i];

        // Get territory at this hex
        TerritoryId tid = state.GetTerritoryAt(coord);
        PlayerId owner = PLAYER_NONE;

        // Set color based on territory owner
        if (tid != TERRITORY_NONE)
        {
            const TerritoryData* territory = state.GetTerritory(tid);
            if (territory && territory->owner != PLAYER_NONE)
            {
                owner = territory->owner;
                const PlayerData& player = state.GetPlayer(owner);
                tile.r = player.colorR;
                tile.g = player.colorG;
                tile.b = player.colorB;
                tile.a = 1.0f;
            }
            else
            {
                // Unowned territory - gray
                tile.r = 0.4f;
                tile.g = 0.4f;
                tile.b = 0.4f;
                tile.a = 1.0f;
            }
        }
        else
        {
            // No territory (shouldn't happen if grid matches)
            tile.r = 0.2f;
            tile.g = 0.2f;
            tile.b = 0.2f;
            tile.a = 1.0f;
        }

        // Calculate per-edge border flags
        // Clear old border bits (bits 4-15)
        tile.flags &= 0xF;  // Keep only bits 0-3 (UI flags)

        if (tid != TERRITORY_NONE)
        {
            // Check each of 6 neighbor directions
            for (int dir = 0; dir < 6; dir++)
            {
                HexCoord neighborCoord = coord.Neighbor(dir);
                TerritoryId neighborTid = state.GetTerritoryAt(neighborCoord);

                // Check if this edge borders a different territory
                if (neighborTid != tid)
                {
                    // Set territory border bit for this edge
                    tile.flags |= (1 << (HEX_BORDER_EDGE_SHIFT + dir));

                    // Check if it's an enemy border (different owner)
                    if (neighborTid != TERRITORY_NONE)
                    {
                        const TerritoryData* neighborTerritory = state.GetTerritory(neighborTid);
                        if (neighborTerritory && neighborTerritory->owner != owner)
                        {
                            // Set enemy border bit for this edge
                            tile.flags |= (1 << (HEX_ENEMY_EDGE_SHIFT + dir));
                        }
                    }
                    else
                    {
                        // Edge of map - treat as enemy border for visibility
                        tile.flags |= (1 << (HEX_ENEMY_EDGE_SHIFT + dir));
                    }
                }
            }
        }
    }

    _isDirty = true;
}

void HexMapData::UpdateTileHighlight(HexTileGPU& tile, uint32_t uiFlags)
{
    // Preserve border bits (4-15), update UI flags (0-3)
    tile.flags = (tile.flags & 0xFFF0) | (uiFlags & 0xF);

    // Set highlight color based on state
    if (uiFlags & HEX_FLAG_SELECTED)
    {
        tile.highlightR = 1.0f;
        tile.highlightG = 1.0f;
        tile.highlightB = 1.0f;
        tile.highlightA = 0.3f;
    }
    else if (uiFlags & HEX_FLAG_VALID_TARGET)
    {
        tile.highlightR = 1.0f;
        tile.highlightG = 0.3f;
        tile.highlightB = 0.3f;
        tile.highlightA = 0.3f;
    }
    else if (uiFlags & HEX_FLAG_HOVERED)
    {
        tile.highlightR = 1.0f;
        tile.highlightG = 1.0f;
        tile.highlightB = 1.0f;
        tile.highlightA = 0.15f;
    }
    else
    {
        tile.highlightA = 0.0f;
    }
}

void HexMapData::UpdateFromGameState(
    const HexGrid& grid,
    const GameState& state,
    const UIState& ui)
{
    ZoneScoped;

    // Check if UI state actually changed
    bool selectedChanged = (ui.selectedHexes != _cachedSelectedHexes);
    bool hoverChanged = (ui.hoverHexes != _cachedHoverHexes);
    bool targetChanged = (ui.validTargetHexes != _cachedTargetHexes);

    if (!selectedChanged && !hoverChanged && !targetChanged)
    {
        return; // Nothing changed, skip update
    }

    // Clear highlights on previously highlighted tiles
    for (const auto& coord : _cachedSelectedHexes)
    {
        auto it = _coordToIndex.find(coord);
        if (it != _coordToIndex.end())
        {
            UpdateTileHighlight(_tiles[it->second], 0);
        }
    }
    for (const auto& coord : _cachedHoverHexes)
    {
        auto it = _coordToIndex.find(coord);
        if (it != _coordToIndex.end())
        {
            UpdateTileHighlight(_tiles[it->second], 0);
        }
    }
    for (const auto& coord : _cachedTargetHexes)
    {
        auto it = _coordToIndex.find(coord);
        if (it != _coordToIndex.end())
        {
            UpdateTileHighlight(_tiles[it->second], 0);
        }
    }

    // Build sets for new UI state (only if we have overlapping highlights)
    std::unordered_set<HexCoord, HexCoordHash> selectedSet(
        ui.selectedHexes.begin(), ui.selectedHexes.end());
    std::unordered_set<HexCoord, HexCoordHash> targetSet(
        ui.validTargetHexes.begin(), ui.validTargetHexes.end());
    std::unordered_set<HexCoord, HexCoordHash> hoverSet(
        ui.hoverHexes.begin(), ui.hoverHexes.end());

    // Apply new highlights - process all currently highlighted hexes
    auto applyHighlight = [&](const HexCoord& coord) {
        auto it = _coordToIndex.find(coord);
        if (it == _coordToIndex.end()) return;

        uint32_t flags = 0;
        if (selectedSet.count(coord)) flags |= HEX_FLAG_SELECTED;
        if (hoverSet.count(coord)) flags |= HEX_FLAG_HOVERED;
        if (targetSet.count(coord)) flags |= HEX_FLAG_VALID_TARGET;
        UpdateTileHighlight(_tiles[it->second], flags);
    };

    for (const auto& coord : ui.selectedHexes) applyHighlight(coord);
    for (const auto& coord : ui.hoverHexes) applyHighlight(coord);
    for (const auto& coord : ui.validTargetHexes) applyHighlight(coord);

    // Cache current state
    _cachedSelectedHexes = ui.selectedHexes;
    _cachedHoverHexes = ui.hoverHexes;
    _cachedTargetHexes = ui.validTargetHexes;

    _isDirty = true;
}


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

        // Set color based on territory owner
        if (tid != TERRITORY_NONE)
        {
            const TerritoryData* territory = state.GetTerritory(tid);
            if (territory && territory->owner != PLAYER_NONE)
            {
                const PlayerData& player = state.GetPlayer(territory->owner);
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

        // Set border flag (static - only changes when territories change)
        if (IsOnBorder(coord, grid, state))
        {
            tile.flags |= HEX_FLAG_BORDER;
        }
        else
        {
            tile.flags &= ~HEX_FLAG_BORDER;
        }
    }

    _isDirty = true;
}

void HexMapData::UpdateTileHighlight(HexTileGPU& tile, uint32_t uiFlags)
{
    // Preserve border flag, update UI flags
    tile.flags = (tile.flags & HEX_FLAG_BORDER) | uiFlags;

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

bool HexMapData::IsOnBorder(
    const HexCoord& coord,
    const HexGrid& grid,
    const GameState& state) const
{
    TerritoryId myTerritory = state.GetTerritoryAt(coord);
    if (myTerritory == TERRITORY_NONE) return false;

    // Check if any neighbor belongs to a different territory
    for (const auto& neighbor : grid.GetNeighbors(coord))
    {
        TerritoryId neighborTerritory = state.GetTerritoryAt(neighbor);
        if (neighborTerritory != myTerritory)
        {
            return true;
        }
    }

    return false;
}

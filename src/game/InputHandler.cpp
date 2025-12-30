//
// InputHandler.cpp - Game input implementation
//

#include "InputHandler.h"

#include <tracy/Tracy.hpp>

InputHandler::InputHandler(
    GameController* controller,
    Camera* camera,
    UIState* uiState)
    : _controller(controller),
      _camera(camera),
      _uiState(uiState)
{
}

bool InputHandler::HandleEvent(const SDL_Event& event)
{
    ZoneScoped;
    GameState& state = _controller->GetState();

    // Don't handle input during AI turn or game over
    if (state.phase == TurnPhase::AITurn ||
        state.phase == TurnPhase::GameOver)
    {
        return false;
    }

    switch (event.type)
    {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                _mouseDown = true;
                HandleClick(event.button.x, event.button.y);
                return true;
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                HandleRightClick();
                return true;
            }
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                _mouseDown = false;
            }
            break;
        }

        case SDL_EVENT_MOUSE_MOTION:
        {
            UpdateHover(event.motion.x, event.motion.y);
            _lastMousePos = {event.motion.x, event.motion.y};
            break;
        }

        case SDL_EVENT_KEY_DOWN:
        {
            // Space or Enter to end turn
            if (event.key.scancode == SDL_SCANCODE_SPACE ||
                event.key.scancode == SDL_SCANCODE_RETURN)
            {
                if (state.phase == TurnPhase::SelectAttacker ||
                    state.phase == TurnPhase::SelectTarget)
                {
                    _controller->EndTurn();
                    return true;
                }
            }
            // Escape to cancel selection
            else if (event.key.scancode == SDL_SCANCODE_ESCAPE)
            {
                _controller->CancelSelection();
                UpdateUIState();
                return true;
            }
            break;
        }

        default:
            break;
    }

    return false;
}

void InputHandler::HandleClick(float screenX, float screenY)
{
    GameState& state = _controller->GetState();

    // Check if clicking end turn button
    if (screenX >= _uiState->endTurnBtnX &&
        screenX <= _uiState->endTurnBtnX + _uiState->endTurnBtnW &&
        screenY >= _uiState->endTurnBtnY &&
        screenY <= _uiState->endTurnBtnY + _uiState->endTurnBtnH)
    {
        _controller->EndTurn();
        UpdateUIState();
        return;
    }

    // Convert to hex coordinate
    HexCoord hex = ScreenToHex(screenX, screenY);

    // Get territory at hex
    TerritoryId tid = state.GetTerritoryAt(hex);
    if (tid == TERRITORY_NONE) return;

    const TerritoryData* territory = state.GetTerritory(tid);
    if (!territory) return;

    if (state.phase == TurnPhase::SelectAttacker)
    {
        // Try to select this territory
        if (_controller->SelectTerritory(tid))
        {
            UpdateUIState();
        }
    }
    else if (state.phase == TurnPhase::SelectTarget)
    {
        // Check if clicking own territory (to change selection)
        if (territory->owner == state.currentPlayer)
        {
            if (territory->CanAttack())
            {
                _controller->SelectTerritory(tid);
                UpdateUIState();
            }
        }
        else
        {
            // Try to attack
            if (_controller->Attack(tid))
            {
                UpdateUIState();
            }
        }
    }
}

void InputHandler::HandleRightClick()
{
    _controller->CancelSelection();
    UpdateUIState();
}

void InputHandler::UpdateHover(float mouseX, float mouseY)
{
    ZoneScoped;
    HexCoord hex = ScreenToHex(mouseX, mouseY);
    const HexGrid& grid = _controller->GetGrid();

    _uiState->isHovering = grid.IsValid(hex);
    _uiState->hoveredHex = hex;

    if (_uiState->isHovering)
    {
        const GameState& state = _controller->GetState();
        _uiState->hoveredTerritory = state.GetTerritoryAt(hex);

        // Update hover hexes
        _uiState->hoverHexes.clear();
        if (_uiState->hoveredTerritory != TERRITORY_NONE)
        {
            const TerritoryData* t = state.GetTerritory(_uiState->hoveredTerritory);
            if (t)
            {
                _uiState->hoverHexes = t->hexes;
            }
        }
    }
    else
    {
        _uiState->hoveredTerritory = TERRITORY_NONE;
        _uiState->hoverHexes.clear();
    }

    // Check end turn button hover
    _uiState->endTurnHovered =
        mouseX >= _uiState->endTurnBtnX &&
        mouseX <= _uiState->endTurnBtnX + _uiState->endTurnBtnW &&
        mouseY >= _uiState->endTurnBtnY &&
        mouseY <= _uiState->endTurnBtnY + _uiState->endTurnBtnH;
}

void InputHandler::UpdateUIState()
{
    const GameState& state = _controller->GetState();

    // Update selected hexes
    _uiState->selectedHexes.clear();
    if (state.selectedTerritory != TERRITORY_NONE)
    {
        const TerritoryData* t = state.GetTerritory(state.selectedTerritory);
        if (t)
        {
            _uiState->selectedHexes = t->hexes;
        }
    }

    // Update valid target hexes
    _uiState->validTargetHexes.clear();
    for (TerritoryId target : state.validTargets)
    {
        const TerritoryData* t = state.GetTerritory(target);
        if (t)
        {
            _uiState->validTargetHexes.insert(
                _uiState->validTargetHexes.end(),
                t->hexes.begin(),
                t->hexes.end()
            );
        }
    }
}

HexCoord InputHandler::ScreenToHex(float screenX, float screenY)
{
    Vector2 screenPos{screenX, screenY};
    Vector2 worldPos = _camera->ScreenToWorld(screenPos);
    return _controller->GetGrid().WorldToHex(worldPos);
}

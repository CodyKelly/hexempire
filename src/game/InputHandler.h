//
// InputHandler.h - Game input handling
//

#ifndef ATLAS_INPUTHANDLER_H
#define ATLAS_INPUTHANDLER_H

#include "GameData.h"
#include "GameController.h"
#include "../hex/HexGrid.h"
#include "../CameraSystem.h"
#include <SDL3/SDL.h>

class InputHandler
{
public:
    InputHandler(
        GameController* controller,
        Camera* camera,
        UIState* uiState
    );

    // Process SDL event, returns true if event was handled
    bool HandleEvent(const SDL_Event& event);

    // Update hover state based on current mouse position
    void UpdateHover(float mouseX, float mouseY);

    // Update UI state (selection highlights, etc.)
    void UpdateUIState();

private:
    GameController* _controller;
    Camera* _camera;
    UIState* _uiState;

    // Mouse state
    bool _mouseDown = false;
    Vector2 _lastMousePos;

    // Handle mouse click at screen position
    void HandleClick(float screenX, float screenY);
    void HandleRightClick();

    // Convert screen position to hex coordinate
    HexCoord ScreenToHex(float screenX, float screenY);
};

#endif // ATLAS_INPUTHANDLER_H

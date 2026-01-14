//
// UIManager.h - RmlUi integration for game UI
//

#ifndef ATLAS_UIMANAGER_H
#define ATLAS_UIMANAGER_H

#include <SDL3/SDL.h>
#include <RmlUi/Core.h>
#include <functional>
#include <vector>
#include <string>

class ResourceManager;
struct GameState;

// Player stats data for UI display (pure data struct)
struct PlayerUIData {
    int playerId = -1;
    std::string name;
    float colorR = 0, colorG = 0, colorB = 0;
    int largestRegion = 0;
    int totalTerritories = 0;
    int totalDice = 0;
    bool isEliminated = false;
    bool isCurrentPlayer = false;
};

class UIManager {
public:
    UIManager();
    ~UIManager();

    // Initialize RmlUi with SDL3 GPU backend
    bool Initialize(ResourceManager* rm, int width, int height);

    // Shutdown RmlUi
    void Shutdown();

    // Update player stats from game state
    // getLargestRegion is a callback to GameController::FindLargestContiguousRegion
    void UpdatePlayerStats(const GameState& state,
                           const std::function<int(int)>& getLargestRegion);

    // Process SDL events for UI input
    // Returns true if event was consumed by UI
    bool ProcessEvent(SDL_Event& event);

    // Frame lifecycle - call in order during render loop
    void BeginFrame(SDL_GPUCommandBuffer* commandBuffer,
                    SDL_GPUTexture* swapchainTexture,
                    uint32_t width, uint32_t height);
    void Update();
    void Render();
    void EndFrame();

    // Window resize handling
    void OnResize(int width, int height);

private:
    ResourceManager* _resourceManager = nullptr;
    Rml::Context* _context = nullptr;
    Rml::ElementDocument* _statsDoc = nullptr;

    // Player stats cache
    std::vector<PlayerUIData> _playerData;
    int _playerCount = 0;

    // Dirty flag for efficient UI updates
    bool _dirty = true;

    // Refresh the player stats UI elements
    void RefreshStatsUI();
};

#endif // ATLAS_UIMANAGER_H

//
// UIManager.cpp - RmlUi integration for game UI
//

#include "UIManager.h"
#include "../ResourceManager.h"
#include "../game/GameData.h"

#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <RmlUi_Renderer_SDL_GPU.h>

#include <tracy/Tracy.hpp>

#include <algorithm>
#include <sstream>

// Static render and system interfaces (RmlUi requires these to outlive the context)
static RenderInterface_SDL_GPU *s_renderInterface = nullptr;
static SystemInterface_SDL *s_systemInterface = nullptr;

UIManager::UIManager() = default;

UIManager::~UIManager() {
    Shutdown();
}

bool UIManager::Initialize(ResourceManager *rm, int width, int height) {
    ZoneScoped;
    _resourceManager = rm;

    // Create system interface for SDL
    s_systemInterface = new SystemInterface_SDL();
    s_systemInterface->SetWindow(rm->GetWindow());
    Rml::SetSystemInterface(s_systemInterface);

    // Create render interface with SDL GPU backend
    s_renderInterface = new RenderInterface_SDL_GPU(
        rm->GetGPUDevice(),
        rm->GetWindow()
    );
    Rml::SetRenderInterface(s_renderInterface);

    // Initialize RmlUi
    if (!Rml::Initialise()) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initialize RmlUi");
        return false;
    }

    // Load font
    if (!Rml::LoadFontFace("content/ui/fonts/LatoLatin-Regular.ttf")) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load UI font, using fallback");
    }

    // Create context
    _context = Rml::CreateContext("main", Rml::Vector2i(width, height));
    if (!_context) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create RmlUi context");
        return false;
    }

    // Load player stats document
    _statsDoc = _context->LoadDocument("content/ui/player_stats.rml");
    if (!_statsDoc) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load player_stats.rml");
        return false;
    }
    _statsDoc->Show();

    // Reserve space for player data
    _playerData.resize(MAX_PLAYERS);

    return true;
}

void UIManager::Shutdown() {
    if (_statsDoc) {
        _statsDoc->Close();
        _statsDoc = nullptr;
    }

    if (_context) {
        Rml::RemoveContext(_context->GetName());
        _context = nullptr;
    }

    Rml::Shutdown();

    if (s_renderInterface) {
        s_renderInterface->Shutdown();
        delete s_renderInterface;
        s_renderInterface = nullptr;
    }

    delete s_systemInterface;
    s_systemInterface = nullptr;
}

void UIManager::UpdatePlayerStats(const GameState &state,
                                  const std::function<int(int)> &getLargestRegion) {
    ZoneScoped;

    bool needsRefresh = false;

    for (int i = 0; i < state.config.playerCount; i++) {
        const PlayerData &player = state.players[i];
        PlayerUIData &stats = _playerData[i];

        int newLargestRegion = getLargestRegion(i);
        int newTotalTerritories = state.CountTerritoriesOwned(i);
        int newTotalDice = state.CountDiceOwned(i);
        bool newIsCurrentPlayer = (state.currentPlayer == static_cast<PlayerId>(i));

        // Check for changes
        if (stats.largestRegion != newLargestRegion ||
            stats.totalTerritories != newTotalTerritories ||
            stats.totalDice != newTotalDice ||
            stats.isEliminated != player.isEliminated ||
            stats.isCurrentPlayer != newIsCurrentPlayer) {
            needsRefresh = true;
        }

        // Update cached data
        stats.playerId = i;
        stats.name = player.name;
        stats.colorR = player.colorR;
        stats.colorG = player.colorG;
        stats.colorB = player.colorB;
        stats.largestRegion = newLargestRegion;
        stats.totalTerritories = newTotalTerritories;
        stats.totalDice = newTotalDice;
        stats.isEliminated = player.isEliminated;
        stats.isCurrentPlayer = newIsCurrentPlayer;
    }

    _playerCount = state.config.playerCount;

    if (needsRefresh) {
        _dirty = true;
    }
}

bool UIManager::ProcessEvent(SDL_Event &event) {
    if (!_context) return false;
    return !RmlSDL::InputEventHandler(_context, _resourceManager->GetWindow(), event);
}

void UIManager::BeginFrame(SDL_GPUCommandBuffer *commandBuffer,
                           SDL_GPUTexture *swapchainTexture,
                           uint32_t width, uint32_t height) {
    if (s_renderInterface) {
        s_renderInterface->BeginFrame(commandBuffer, swapchainTexture, width, height);
    }
}

void UIManager::Update() {
    ZoneScoped;
    if (!_context) return;

    // Refresh UI elements if data changed
    if (_dirty) {
        RefreshStatsUI();
        _dirty = false;
    }

    _context->Update();
}

void UIManager::Render() {
    ZoneScoped;
    if (!_context) return;
    _context->Render();
}

void UIManager::EndFrame() {
    if (s_renderInterface) {
        s_renderInterface->EndFrame();
    }
}

void UIManager::OnResize(int width, int height) {
    if (_context) {
        _context->SetDimensions(Rml::Vector2i(width, height));
    }
}

void UIManager::RefreshStatsUI() {
    ZoneScoped;
    if (!_statsDoc) return;

    Rml::Element *playerList = _statsDoc->GetElementById("player-list");
    if (!playerList) return;

    // Clear existing entries
    while (playerList->HasChildNodes()) {
        playerList->RemoveChild(playerList->GetFirstChild());
    }

    // Create sorted list by largest region (descending)
    std::vector<const PlayerUIData *> sortedStats;
    for (int i = 0; i < _playerCount; i++) {
        sortedStats.push_back(&_playerData[i]);
    }
    std::sort(sortedStats.begin(), sortedStats.end(),
              [](const PlayerUIData *a, const PlayerUIData *b) {
                  // Sort by largest region, then by total territories
                  if (a->largestRegion != b->largestRegion) {
                      return a->largestRegion > b->largestRegion;
                  }
                  return a->totalTerritories > b->totalTerritories;
              });

    // Create player entries
    for (const PlayerUIData *stats: sortedStats) {
        Rml::ElementPtr entry = _statsDoc->CreateElement("div");
        entry->SetClassNames("player-entry");

        if (stats->isEliminated) {
            entry->SetClass("eliminated", true);
        }
        if (stats->isCurrentPlayer) {
            entry->SetClass("current-player", true);
        }

        // Color indicator
        Rml::ElementPtr colorBox = _statsDoc->CreateElement("div");
        colorBox->SetClassNames("player-color");

        std::ostringstream colorStyle;
        colorStyle << "background-color: rgb("
                << static_cast<int>(stats->colorR * 255) << ", "
                << static_cast<int>(stats->colorG * 255) << ", "
                << static_cast<int>(stats->colorB * 255) << ");";
        colorBox->SetAttribute("style", colorStyle.str());
        entry->AppendChild(std::move(colorBox));

        // Player name
        Rml::ElementPtr nameSpan = _statsDoc->CreateElement("span");
        nameSpan->SetClassNames("player-name");
        nameSpan->SetInnerRML(stats->name);
        entry->AppendChild(std::move(nameSpan));

        // Stats container
        Rml::ElementPtr statsDiv = _statsDoc->CreateElement("div");
        statsDiv->SetClassNames("player-stats");

        // Largest region (main stat)
        Rml::ElementPtr largestSpan = _statsDoc->CreateElement("span");
        largestSpan->SetClassNames("stat-largest");
        largestSpan->SetInnerRML(std::to_string(stats->largestRegion));
        statsDiv->AppendChild(std::move(largestSpan));

        // Territories count (secondary)
        Rml::ElementPtr territoriesSpan = _statsDoc->CreateElement("span");
        territoriesSpan->SetClassNames("stat-territories");
        territoriesSpan->SetInnerRML(std::to_string(stats->totalTerritories) + " terr");
        statsDiv->AppendChild(std::move(territoriesSpan));

        entry->AppendChild(std::move(statsDiv));
        playerList->AppendChild(std::move(entry));
    }
}

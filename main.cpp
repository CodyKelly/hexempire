#define SDL_MAIN_USE_CALLBACKS
#define LogError(error) (SDL_LogError(SDL_LOG_CATEGORY_ERROR, error))

#include <chrono>

#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"

#include <tracy/Tracy.hpp>

#include "src/ResourceManager.h"
#include "src/SpriteBatch.h"
#include "src/CameraSystem.h"

#include "src/hex/HexGrid.h"
#include "src/hex/HexMapData.h"
#include "src/hex/HexMapRenderer.h"

#include "src/game/GameData.h"
#include "src/game/GameController.h"
#include "src/game/AIController.h"
#include "src/game/InputHandler.h"

#include "src/ui/DiceRenderer.h"

// Global systems
ResourceManager resourceManager;
Camera camera;
CameraController cameraController;

// Game systems
GameController *gameController = nullptr;
AIController *aiController = nullptr;
InputHandler *inputHandler = nullptr;

// Rendering
HexMapData *hexMapData = nullptr;
HexMapRenderer *hexMapRenderer = nullptr;
DiceRenderer *diceRenderer = nullptr;

// UI state
UIState uiState;

struct AppState {
    double deltaTime = 0;
    double totalTime = 0;
    double fps = 0;
    bool cameraDragging = false;
    Vector2 lastMousePos;
};

struct VertexUniforms {
    Matrix4x4 viewProjection;
    float time;
    float pad1, pad2, pad3;
};

AppState appState;

uint64_t MilSinceEpoch() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).
            count();
}

void UpdateTime() {
    ZoneScoped;
    static Uint64 lastFrameTime = 0;
    static Uint64 fpsUpdateTime = 0;
    static Uint64 frameCount = 0;

    Uint64 currentTime = SDL_GetTicksNS();

    if (lastFrameTime == 0) {
        lastFrameTime = currentTime;
        fpsUpdateTime = currentTime;
    }

    appState.deltaTime = (currentTime - lastFrameTime) / 1000000000.0;
    appState.totalTime = currentTime / 1000000000.0;
    lastFrameTime = currentTime;
    frameCount++;

    float timeSinceUpdate = (currentTime - fpsUpdateTime) / 1000000000.0f;
    if (timeSinceUpdate >= 1.0f) {
        appState.fps = frameCount / timeSinceUpdate;

        const GameState &state = gameController->GetState();
        const char *phaseStr = "";
        switch (state.phase) {
            case TurnPhase::SelectAttacker: phaseStr = "Select Attacker";
                break;
            case TurnPhase::SelectTarget: phaseStr = "Select Target";
                break;
            case TurnPhase::AITurn: phaseStr = "AI Thinking...";
                break;
            case TurnPhase::GameOver: phaseStr = "GAME OVER";
                break;
            default: phaseStr = "";
                break;
        }

        char title[256];
        SDL_snprintf(title, sizeof(title), "Hex Empire - Turn %d - %s - FPS: %.1f",
                     state.turnNumber, phaseStr, appState.fps);
        SDL_SetWindowTitle(resourceManager.GetWindow(), title);

        frameCount = 0;
        fpsUpdateTime = currentTime;
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    ZoneScoped;
    resourceManager.Init("Hex Empire", 1200, 900, SDL_WINDOW_RESIZABLE);

    // Create graphics pipelines
    resourceManager.CreateGraphicsPipeline("sprites",
                                           {"./content/shaders/sprite.vert.hlsl", 0, 1, 1, 0},
                                           {"./content/shaders/sprite.frag.hlsl", 1, 0, 0, 0}
    );

    resourceManager.CreateGraphicsPipeline("hexTiles",
                                           {"./content/shaders/hex_tile.vert.hlsl", 0, 1, 1, 0},
                                           {"./content/shaders/hex_tile.frag.hlsl", 0, 0, 0, 0}
    );

    // Create sampler
    constexpr SDL_GPUSamplerCreateInfo samplerInfo = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    auto sampler = resourceManager.CreateSampler("default", &samplerInfo);

    // Load texture
    auto texture = resourceManager.CreateTexture("atlas", "./content/textures/dice.png");
    if (!texture) {
        LogError("Failed to load dice texture");
        return SDL_APP_FAILURE;
    }

    // Initialize game controller
    gameController = new GameController();

    // Configure game
    GameConfig config;
    config.gridRadius = 20;
    config.playerCount = 8;
    config.humanPlayerIndex = 0;
    config.targetTerritoryCount = 45;
    config.startingDicePerPlayer = 12;
    config.hexSize = 24.0f;
    config.seed = MilSinceEpoch(); // Random seed
    config.fillHoles = false;
    config.keepLargestIslandOnly = true;

    gameController->InitializeGame(config);

    // Initialize AI controller
    aiController = new AIController(gameController);
    gameController->SetAIController(aiController);

    // Initialize hex map rendering
    const HexGrid &grid = gameController->GetGrid();
    const GameState &initialState = gameController->GetState();
    hexMapData = new HexMapData();
    hexMapData->Initialize(grid);
    hexMapData->UpdateFromTerritories(grid, initialState);

    hexMapRenderer = new HexMapRenderer(&resourceManager);
    hexMapRenderer->Initialize(grid.GetHexCount());
    hexMapRenderer->SetHexMapData(hexMapData);

    // Initialize dice renderer
    diceRenderer = new DiceRenderer(&resourceManager);
    diceRenderer->Initialize(1000, texture, sampler); // Max 1000 dice sprites

    // Initialize camera
    int windowWidth, windowHeight;
    SDL_GetWindowSize(resourceManager.GetWindow(), &windowWidth, &windowHeight);
    camera = Camera({(float) windowWidth, (float) windowHeight});

    // Center camera on grid
    Vector2 gridCenter = grid.GetWorldCenter();
    camera.SetPosition(gridCenter);
    camera.SetScale({1.0f, 1.0f});

    cameraController = CameraController(&camera, {
                                            .zoomMin = 0.03f,
                                            .zoomMax = 30.0f,
                                            .zoomSpeed = 0.1f,
                                            .moveSpeed = 500.0f,
                                            .smoothing = 8.0f
                                        });

    // Initialize input handler
    inputHandler = new InputHandler(gameController, &camera, &uiState);

    // Set up UI state
    uiState.endTurnBtnX = 20;
    uiState.endTurnBtnY = 20;
    uiState.endTurnBtnW = 100;
    uiState.endTurnBtnH = 30;

    // Initial UI update
    inputHandler->UpdateUIState();

    SDL_Log("Game initialized with %zu territories", gameController->GetState().territories.size());

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    ZoneScoped;
    UpdateTime();

    // Update camera viewport to match window
    int windowWidth, windowHeight;
    SDL_GetWindowSize(resourceManager.GetWindow(), &windowWidth, &windowHeight);
    camera.SetViewportSize({(float) windowWidth, (float) windowHeight});

    // Update camera smoothing
    cameraController.Update((float) appState.deltaTime);

    // Update game logic
    gameController->Update((float) appState.deltaTime);

    // Update rendering data
    const GameState &state = gameController->GetState();
    const HexGrid &grid = gameController->GetGrid();

    // Mark hex map dirty if territory ownership changed
    if (state.mapNeedsRefresh) {
        hexMapData->MarkDirty();
        hexMapData->UpdateFromTerritories(grid, state); // Sync territory colors
        gameController->GetState().mapNeedsRefresh = false;
    }


    hexMapData->UpdateFromGameState(grid, state, uiState);
    diceRenderer->UpdateFromGameState(state, grid);

    // Acquire command buffer
    SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(resourceManager.GetGPUDevice());
    if (!commandBuffer) return SDL_APP_FAILURE;

    SDL_GPUTexture *swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer,
                                               resourceManager.GetWindow(), &swapchainTexture, nullptr, nullptr)) {
        return SDL_APP_FAILURE;
    }

    if (swapchainTexture) {
        // Upload data to GPU
        hexMapRenderer->Upload(commandBuffer);
        diceRenderer->Upload(commandBuffer);

        // Begin render pass
        SDL_GPUColorTargetInfo colorTarget = {
            .texture = swapchainTexture,
            .clear_color = {0.15f, 0.15f, 0.2f, 1.0f},
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE
        };

        SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);

        // Push uniforms
        VertexUniforms uniforms = {
            camera.GetViewProjectionMatrix(),
            (float) appState.totalTime,
            0, 0, 0
        };

        SDL_PushGPUVertexUniformData(
            commandBuffer,
            0,
            &uniforms,
            sizeof(uniforms)
        );

        // Draw hex map
        hexMapRenderer->Draw(renderPass);

        // Draw dice
        diceRenderer->Draw(renderPass);

        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(commandBuffer);

    FrameMark;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            // Middle mouse for camera pan
            if (event->button.button == SDL_BUTTON_MIDDLE) {
                appState.cameraDragging = true;
                appState.lastMousePos = {event->button.x, event->button.y};
            }
            // Left/right click handled by input handler
            else {
                inputHandler->HandleEvent(*event);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button == SDL_BUTTON_MIDDLE) {
                appState.cameraDragging = false;
            } else {
                inputHandler->HandleEvent(*event);
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            // Camera pan with middle mouse
            if (appState.cameraDragging) {
                Vector2 currentPos = {event->motion.x, event->motion.y};
                Vector2 delta = appState.lastMousePos - currentPos;
                cameraController.Pan(delta);
                appState.lastMousePos = currentPos;
            }
            // Update hover state
            inputHandler->UpdateHover(event->motion.x, event->motion.y);
            break;

        case SDL_EVENT_MOUSE_WHEEL: {
            float mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);
            cameraController.ZoomToPoint(event->wheel.y, {mouseX, mouseY});
        }
        break;

        case SDL_EVENT_KEY_DOWN:
            inputHandler->HandleEvent(*event);

            // R to restart game
            if (event->key.scancode == SDL_SCANCODE_R) {
                GameConfig config = gameController->GetState().config;
                config.seed = MilSinceEpoch(); // New random seed
                gameController->InitializeGame(config);
                aiController = new AIController(gameController);
                gameController->SetAIController(aiController);
                const HexGrid &restartGrid = gameController->GetGrid();
                hexMapData->Initialize(restartGrid);
                hexMapData->UpdateFromTerritories(restartGrid, gameController->GetState());
                inputHandler->UpdateUIState();
                SDL_Log("Game restarted");
            }
            break;

        default:
            break;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    delete inputHandler;
    delete aiController;
    delete gameController;
    delete diceRenderer;
    delete hexMapRenderer;
    delete hexMapData;
}

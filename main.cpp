#define SDL_MAIN_USE_CALLBACKS
#define LogError(error) (SDL_LogError(SDL_LOG_CATEGORY_ERROR, error))

#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"

#include "src/ResourceManager.h"
#include "src/SpriteBatch.h"
#include "src/CameraSystem.h"

ResourceManager resourceManager;
SpriteBatch *spriteBatch = nullptr;
Camera camera;
CameraController cameraController;

struct AppState {
    double deltaTime = 0;
    double totalTime = 0;
    double fps = 0;
    bool mouseDown = false;
    Vector2 lastMousePos;
};

AppState appState;

void UpdateTime() {
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

        char title[256];
        SDL_snprintf(title, sizeof(title), "ATLAS - FPS: %.1f", appState.fps);
        SDL_SetWindowTitle(resourceManager.GetWindow(), title);

        frameCount = 0;
        fpsUpdateTime = currentTime;
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    resourceManager.Init("ATLAS", 1000, 1000, SDL_WINDOW_RESIZABLE);

    resourceManager.CreateGraphicsPipeline("sprites",
                                           {"./shaders/sprite.vert.hlsl", 0, 1, 1, 0},
                                           {"./shaders/sprite.frag.hlsl", 1, 0, 0, 0}
    );

    constexpr SDL_GPUSamplerCreateInfo samplerInfo = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    auto sampler = resourceManager.CreateSampler("spriteSampler", &samplerInfo);

    // Texture loading is now encapsulated in ResourceManager
    auto texture = resourceManager.CreateTexture("sprite_atlas", "./Content/Textures/atlas.png");
    if (!texture) {
        return SDL_APP_FAILURE;
    }

    spriteBatch = new SpriteBatch("sprites", &resourceManager, 1000000);
    spriteBatch->SetTexture(texture, sampler);

    spriteBatch->AddSprite({
        0, 0, 0,
        0,
        640, 400, 0, 0,
        0.0f, 0.0f, 1.0f, 1.0f,
        1, 1, 1, 1
    });

    // Initialize camera
    camera = Camera({1000, 1000});
    cameraController = CameraController(&camera, {
                                            .zoomMin = 0.1f,
                                            .zoomMax = 10.0f,
                                            .zoomSpeed = 0.1f,
                                            .moveSpeed = 500.0f,
                                            .smoothing = 8.0f
                                        });

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    UpdateTime();

    // Update camera viewport to match window
    int windowWidth, windowHeight;
    SDL_GetWindowSize(resourceManager.GetWindow(), &windowWidth, &windowHeight);
    camera.SetViewportSize({(float) windowWidth, (float) windowHeight});

    // Update camera smoothing
    cameraController.Update((float) appState.deltaTime);

    // Acquire command buffer
    SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(resourceManager.GetGPUDevice());
    if (!commandBuffer) return SDL_APP_FAILURE;

    SDL_GPUTexture *swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer,
                                               resourceManager.GetWindow(), &swapchainTexture, nullptr, nullptr)) {
        return SDL_APP_FAILURE;
    }

    if (swapchainTexture) {
        spriteBatch->Upload(commandBuffer);

        SDL_GPUColorTargetInfo colorTarget = {
            .texture = swapchainTexture,
            .clear_color = {0.25f, 0.25f, 1.0f, 1.0f},
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE
        };

        SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
        spriteBatch->Draw(renderPass, commandBuffer, camera.GetViewProjectionMatrix());
        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(commandBuffer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    const static auto mouseBtn = SDL_BUTTON_LEFT;
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.button == mouseBtn) {
                appState.mouseDown = true;
                appState.lastMousePos = {event->button.x, event->button.y};
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button == mouseBtn) {
                appState.mouseDown = false;
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (appState.mouseDown) {
                Vector2 currentPos = {event->motion.x, event->motion.y};
                Vector2 delta = appState.lastMousePos - currentPos;
                cameraController.Pan(delta);
                appState.lastMousePos = currentPos;
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL: {
            float mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);
            cameraController.ZoomToPoint(event->wheel.y, {mouseX, mouseY});
        }
        break;

        case SDL_EVENT_KEY_DOWN:
            // Add keyboard controls as needed
            break;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    delete spriteBatch;
}

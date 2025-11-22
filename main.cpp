#define SDL_MAIN_USE_CALLBACKS

#define LogError(error) (SDL_LogError(SDL_LOG_CATEGORY_ERROR, error))

#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"
#include "SDL3_shadercross/SDL_shadercross.h"

#include "src/ResourceManager.h"
#include "src/SpriteBatch.h"
#include "src/math.h"

ResourceManager resourceManager;

SDL_GPUCommandBuffer* commandBuffer;
SDL_GPUTexture* swapchainTexture;
SDL_GPUBuffer* spriteBuffer;
SDL_GPUTransferBuffer* spriteTransferBuffer;

SpriteBatch* spriteBatch;

struct TimeInfo
{
  double deltaTime;
  double totalTime;
  double fps;
};

constexpr uint32_t SPRITE_COUNT = 10000;

Matrix4x4 cameraMatrix = Matrix4x4_CreateOrthographicOffCenter(
  0,
  1000,
  1000,
  0,
  0,
  -1);

TimeInfo updateTime()
{
  static Uint64 lastFrameTime = 0;
  static Uint64 fpsUpdateTime = 0;
  static Uint64 frameCount = 0;
  static float fps = 0.0f;

  Uint64 currentTime = SDL_GetTicksNS(); // Nanoseconds since SDL_Init

  // Initialize on first call
  if (lastFrameTime == 0)
  {
    lastFrameTime = currentTime;
    fpsUpdateTime = currentTime;
  }

  // Calculate delta time in seconds
  float deltaTime = (currentTime - lastFrameTime) / 1000000000.0f;
  float totalTime = currentTime / 1000000000.0f; // Total time since init
  lastFrameTime = currentTime;

  frameCount++;

  // Update FPS every second
  float timeSinceUpdate = (currentTime - fpsUpdateTime) / 1000000000.0f;
  if (timeSinceUpdate >= 1.0f)
  {
    fps = frameCount / timeSinceUpdate;

    char title[256];
    SDL_snprintf(title, sizeof(title), "QUADS - FPS: %.1f", fps);
    SDL_SetWindowTitle(resourceManager.GetWindow(), title);

    frameCount = 0;
    fpsUpdateTime = currentTime;
  }

  return {deltaTime, totalTime, fps};
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
  resourceManager.Init("QUADS", 1000, 1000, SDL_WINDOW_RESIZABLE);

  resourceManager.CreateGraphicsPipeline("sprites",
                                         {"./shaders/triangle.vert.hlsl", 0, 1, 1, 0},
                                         {"./shaders/triangle.frag.hlsl", 0, 0, 0, 0}
  );
  spriteBatch = new SpriteBatch(&resourceManager, 1000000);

  for (int i = 0; i < SPRITE_COUNT; i++)
  {
    spriteBatch->AddSprite(rand() % 1000, rand() % 1000, 10.0f);
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  auto [deltaTime, totalTime, fps] = updateTime();

  // Get current window size
  int windowWidth, windowHeight;
  SDL_GetWindowSize(resourceManager.GetWindow(), &windowWidth, &windowHeight);

  Matrix4x4 cameraMatrix = Matrix4x4_CreateOrthographicOffCenter(
    0, windowWidth, windowHeight, 0, 0, -1
  );

  float centerX = windowWidth / 2.0f;
  float centerY = windowHeight / 2.0f;

  SpriteInstance* sprites = spriteBatch->GetSpriteData();
  size_t spriteCount = spriteBatch->GetSpriteCount();

  for (size_t i = 0; i < spriteCount; i++)
  {
    float spiralSpeed = 0.25f;
    float spiralTightness = 500.0f / SPRITE_COUNT;
    float baseAngle = i * 0.05f;

    float angle = baseAngle + totalTime * spiralSpeed;
    float radius = i * spiralTightness;

    sprites[i].x = centerX + SDL_cosf(angle) * radius;
    sprites[i].y = centerY + SDL_sinf(angle) * radius;
    sprites[i].rotation = angle;

    float t = (float)i / spriteCount * SDL_sinf(totalTime / 6.0f);
    sprites[i].r = 0.5f + 0.5f * SDL_sinf(t * 6.28f);
    sprites[i].g = 0.5f + 0.5f * SDL_sinf(t * 6.28f + 2.09f);
    sprites[i].b = 0.5f + 0.5f * SDL_sinf(t * 6.28f + 4.18f);
    sprites[i].a = 1.0f;
  }

  // Mark as dirty after bulk update
  spriteBatch->MarkDirty();

  commandBuffer = SDL_AcquireGPUCommandBuffer(resourceManager.GetGPUDevice());
  if (!commandBuffer) return SDL_APP_FAILURE;

  if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer,
                                             resourceManager.GetWindow(), &swapchainTexture, nullptr, nullptr))
  {
    return SDL_APP_FAILURE;
  }

  if (swapchainTexture)
  {
    spriteBatch->Upload(commandBuffer);

    SDL_GPUColorTargetInfo colorTarget = {
      .texture = swapchainTexture,
      .clear_color = {0.25f, 0.25f, 1.0f, 1.0f},
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE
    };

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
      commandBuffer, &colorTarget, 1, NULL);
    spriteBatch->Draw(renderPass, commandBuffer, cameraMatrix);

    SDL_EndGPURenderPass(renderPass);
  }

  SDL_SubmitGPUCommandBuffer(commandBuffer);
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
  switch (event->type)
  {
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
    break;

  case SDL_EVENT_KEY_DOWN:
    switch (event->key.scancode)
    {
    case SDL_SCANCODE_LEFT:
      break;

    case SDL_SCANCODE_RIGHT:
      break;

    default:
      break;
    }
    break;

  default:
    break;
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
  delete spriteBatch;
}

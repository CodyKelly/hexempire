#define SDL_MAIN_USE_CALLBACKS

#define LogError(error) (SDL_LogError(SDL_LOG_CATEGORY_ERROR, error))

#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"
#include "SDL3_shadercross/SDL_shadercross.h"

#include "src/ResourceManager.h"

ResourceManager resourceManager;

SDL_GPUGraphicsPipeline* pipeline;
SDL_GPUCommandBuffer* commandBuffer;
SDL_GPUTexture* swapchainTexture;

struct GameTime
{
  double deltaTime;
  double accumulatedTime;
  double fps;
};

GameTime updateTime()
{
  // Get current time
  static Uint64 currentTime;
  static Uint64 frequency;
  static Uint64 frameCount = 0;
  static Uint64 lastFPSUpdate = 0;
  static double accumulatedTime = 0.0;
  static double deltaTime;
  static double fps;

  currentTime = SDL_GetPerformanceCounter();
  frequency = SDL_GetPerformanceFrequency();

  // Initialize lastFPSUpdate on first frame
  if (lastFPSUpdate == 0)
  {
    lastFPSUpdate = currentTime;
  }

  // Calculate delta time
  deltaTime = static_cast<double>(currentTime - lastFPSUpdate) / frequency;
  accumulatedTime += deltaTime;
  frameCount++;

  // Update FPS every second
  if (accumulatedTime >= 1.0)
  {
    fps = frameCount / accumulatedTime;

    // Update window title
    char title[256];
    SDL_snprintf(title, sizeof(title), "Hello Buffers - FPS: %.1f", fps);
    SDL_SetWindowTitle(window, title);

    // Reset counters
    frameCount = 0;
    accumulatedTime = 0.0;
  }

  lastFPSUpdate = currentTime;

  return {deltaTime, accumulatedTime, fps};
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
  resourceManager.Init("Game", 800, 600, true ? SDL_WINDOW_RESIZABLE : 0);

  resourceManager.CreateGraphicsPipeline(
    "sprites",
    {
      "./shaders/triangle.vert.hlsl",
      0, 1, 1, 0
    }, {
      "./shaders/triangle.frag.hlsl",
      0, 0, 0, 0
    }
  );

  spriteBuffers = new GPUBufferPair(sizeof(SpriteInstance) * SPRITE_COUNT, gpuDevice);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  auto [deltaTime, accumulatedTime, fps] = updateTime();

  commandBuffer = SDL_AcquireGPUCommandBuffer(gpuDevice);
  if (!commandBuffer)
  {
    LogError("SDL_AcquireGPUCommandBuffer failed");
    return SDL_APP_FAILURE;
  }

  if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, window, &swapchainTexture, nullptr, nullptr))
  {
    LogError("SDL_WaitAndAcquireGPUSwapchainTexture failed");
    return SDL_APP_FAILURE;
  }

  if (swapchainTexture)
  {
    SpriteInstance* dataPtr = (SpriteInstance*)SDL_MapGPUTransferBuffer(
      gpuDevice,
      spriteBuffers->_transferBuffer,
      true);

    const float gridSpacing = 0.1f;
    const float shapeScale = 4.0f;

    // Compute grid dimensions in pixels
    float gridWidth = gridSpacing;
    float gridHeight = gridSpacing;

    for (int i = 0; i < SPRITE_COUNT; i++)
    {
      int row = i * gridHeight;
      int col = i * gridWidth;
      float angle = accumulatedTime * i * 100.0f;
      // Center offset
      float offsetX = 2 * i;
      float offsetY = 3 * i;
      dataPtr[i].x = 400 + SDL_cosf(angle) * 200 + offsetX;
      dataPtr[i].y = 300 + SDL_sinf(angle) * 200 + offsetY;
      dataPtr[i].rotation = angle;
      dataPtr[i].scale = 100.0f;
      dataPtr[i].r = 1.0f;
      dataPtr[i].g = 1.0f;
      dataPtr[i].b = 1.0f;
      dataPtr[i].a = 1.0f;

      if (row == 0 || col == 0 || row == 1 || col == 1)
      {
        dataPtr[i].g = 0.0f;
        dataPtr[i].b = 0.0f;
      }
    }

    SDL_UnmapGPUTransferBuffer(gpuDevice, spriteBuffers->_transferBuffer);

    // Upload shape data
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    SDL_GPUTransferBufferLocation shapeTransferBufferLocation = {
      .transfer_buffer = spriteBuffers->_transferBuffer,
      .offset = 0,
    };
    SDL_GPUBufferRegion shapeBufferRegion = {
      .buffer = spriteBuffers->_dataBuffer,
      .offset = 0,
      .size = SPRITE_COUNT * sizeof(SpriteInstance),
    };
    SDL_UploadToGPUBuffer(
      copyPass,
      &shapeTransferBufferLocation,
      &shapeBufferRegion,
      true);
    SDL_EndGPUCopyPass(copyPass);

    SDL_GPUColorTargetInfo colorTargetInfo = {
      .texture = swapchainTexture,
      .clear_color = (SDL_FColor){0.25f, 0.25f, 1.0f, 1.0f},
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE
    };
    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
      commandBuffer,
      &colorTargetInfo,
      1,
      NULL);

    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
    SDL_BindGPUVertexStorageBuffers(
      renderPass,
      0,
      &spriteBuffers->_dataBuffer,
      1);
    SDL_PushGPUVertexUniformData(
      commandBuffer,
      0,
      &cameraMatrix,
      sizeof(Matrix4x4));
    SDL_DrawGPUPrimitives(
      renderPass,
      SPRITE_COUNT * 6,
      1,
      0,
      0);

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
  spriteBuffers->ReleaseBuffers();
  SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipeline);
  SDL_ReleaseWindowFromGPUDevice(gpuDevice, window);
  SDL_DestroyGPUDevice(gpuDevice);
  SDL_DestroyWindow(window);
}

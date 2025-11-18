#define SDL_MAIN_USE_CALLBACKS

#define LogError(error) (SDL_LogError(SDL_LOG_CATEGORY_ERROR, error))

#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"
#include "SDL3_shadercross/SDL_shadercross.h"

class GPUBufferPair {
public:
  SDL_GPUTransferBuffer *_transferBuffer;
  SDL_GPUBuffer *_dataBuffer;
  SDL_GPUDevice *_gpuDevice;

  GPUBufferPair(Uint32 size, SDL_GPUDevice *gpuDevice) : _gpuDevice(gpuDevice) {
    SDL_GPUTransferBufferCreateInfo shapeTransferBufferCreateInfo = {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = size,
    };
    _transferBuffer = SDL_CreateGPUTransferBuffer(_gpuDevice, &shapeTransferBufferCreateInfo);

    SDL_GPUBufferCreateInfo shapeBufferCreateInfo = {
      .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
      .size = size,
    };
    _dataBuffer = SDL_CreateGPUBuffer(_gpuDevice, &shapeBufferCreateInfo);
  }

  ~GPUBufferPair() {
    ReleaseBuffers();
  }

  void ReleaseBuffers() {
    SDL_ReleaseGPUBuffer(_gpuDevice, _dataBuffer);
    SDL_ReleaseGPUTransferBuffer(_gpuDevice, _transferBuffer);
  }
};

const uint32_t SPRITE_COUNT = 10000;

static Uint64 frameCount = 0;
static Uint64 lastFPSUpdate = 0;
static double accumulatedTime = 0.0;

typedef struct Matrix4x4 {
  float m11, m12, m13, m14;
  float m21, m22, m23, m24;
  float m31, m32, m33, m34;
  float m41, m42, m43, m44;
} Matrix4x4;

Matrix4x4 Matrix4x4_CreateOrthographicOffCenter(
  float left,
  float right,
  float bottom,
  float top,
  float zNearPlane,
  float zFarPlane) {
  return (Matrix4x4){
    2.0f / (right - left), 0, 0, 0,
    0, 2.0f / (top - bottom), 0, 0,
    0, 0, 1.0f / (zNearPlane - zFarPlane), 0,
    (left + right) / (left - right), (top + bottom) / (bottom - top), zNearPlane / (zNearPlane - zFarPlane), 1
  };
}

SDL_GPUGraphicsPipeline *pipeline;
SDL_GPUCommandBuffer *commandBuffer;
SDL_GPUTexture *swapchainTexture;
SDL_GPUDevice *gpuDevice;
SDL_Window *window;
GPUBufferPair *spriteBuffers;
Matrix4x4 cameraMatrix = Matrix4x4_CreateOrthographicOffCenter(
  0,
  800,
  600,
  0,
  0,
  -1);

SDL_GPUShader *LoadShader(
  SDL_GPUDevice *device,
  const char *shaderFilename,
  const char *basePath,
  Uint32 samplerCount,
  Uint32 uniformBufferCount,
  Uint32 storageBufferCount,
  Uint32 storageTextureCount) {
  if (!device) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "LoadShader: Device error");
    return NULL;
  }
  if (!shaderFilename) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "LoadShader: Shader filename error");
    return NULL;
  }
  if (!basePath) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "LoadShader: Base path error");
    return NULL;
  }

  // Auto-detect the shader stage from the file name for convenience
  SDL_GPUShaderStage stage;
  SDL_ShaderCross_ShaderStage crossStage;
  if (SDL_strstr(shaderFilename, ".vert")) {
    stage = SDL_GPU_SHADERSTAGE_VERTEX;
    crossStage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
  } else if (SDL_strstr(shaderFilename, ".frag")) {
    stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    crossStage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
  } else {
    SDL_Log("Invalid shader stage!");
    return NULL;
  }

  // Build full path with bounds checking
  char fullPath[1024];
  int result = SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, shaderFilename);
  if (result < 0 || result >= sizeof(fullPath)) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "LoadShader: Path too long for shader '%s'", shaderFilename);
    return NULL;
  }

  // Load shader source with size validation
  size_t hlslSourceSize;
  void *hlslSource = SDL_LoadFile(fullPath, &hlslSourceSize);
  if (!hlslSource) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "LoadShader: Failed to load shader from '%s' - %s", fullPath, SDL_GetError());
    return NULL;
  }

  SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
  SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
  const char *entrypoint;

  if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
    SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/SPIRV/%s.spv", basePath, shaderFilename);
    format = SDL_GPU_SHADERFORMAT_SPIRV;
    entrypoint = "main";
  } else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
    SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/MSL/%s.msl", basePath, shaderFilename);
    format = SDL_GPU_SHADERFORMAT_MSL;
    entrypoint = "main0";
  }

  SDL_ShaderCross_HLSL_Info hlslInfo = {
    .source = static_cast<const char *>(hlslSource),
    .entrypoint = "main",
    .include_dir = nullptr,
    .defines = nullptr,
    .shader_stage = crossStage,
  };

  size_t codeSize;
  void *code = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &codeSize);

  switch (format) {
    case SDL_GPU_SHADERFORMAT_SPIRV:
      break;
    case SDL_GPU_SHADERFORMAT_MSL: {
      auto shaderCrossStage = static_cast<SDL_ShaderCross_ShaderStage>(stage);
      auto spirvInfo = SDL_ShaderCross_SPIRV_Info{
        static_cast<const unsigned char *>(code),
        codeSize,
        "main",
        shaderCrossStage,
      };
      auto mslCode = SDL_ShaderCross_TranspileMSLFromSPIRV(&spirvInfo);
      SDL_Log("%s", static_cast<const char *>(mslCode));
      SDL_free(code);
      code = mslCode;
      codeSize = SDL_strlen(static_cast<const char *>(code));
      break;
    }
    default:
      LogError("Unsupported shader format");
  }

  if (!code) {
    SDL_Log("Failed to compile HLSL: %s", SDL_GetError());
    SDL_free(hlslSource);
    return nullptr;
  }

  SDL_free(hlslSource);

  SDL_GPUShaderCreateInfo shaderInfo;
  shaderInfo.stage = stage;
  shaderInfo.format = format;
  shaderInfo.code = (const Uint8 *) code;
  shaderInfo.code_size = codeSize;
  shaderInfo.entrypoint = entrypoint;
  shaderInfo.num_samplers = samplerCount;
  shaderInfo.num_uniform_buffers = uniformBufferCount;
  shaderInfo.num_storage_buffers = storageBufferCount;
  shaderInfo.num_storage_textures = storageTextureCount;
  SDL_GPUShader *shader = SDL_CreateGPUShader(device, &shaderInfo);
  if (shader == NULL) {
    SDL_Log("Failed to create shader!");
    SDL_free(code);
    return NULL;
  }

  SDL_free(code);

  SDL_Log("Successfully created GPU shader for %s", shaderFilename);
  return shader;
}

struct SpriteInstance {
  float x, y, z, rotation;
  float scale, padding1, padding2, padding3;
  float r, g, b, a;
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  SDL_ShaderCross_Init();

  gpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true, NULL);
  if (!gpuDevice) {
    LogError("SDL_CreateGPUDevice failed");
    return SDL_APP_FAILURE;
  }

  window = SDL_CreateWindow("title", 800, 600, true ? SDL_WINDOW_RESIZABLE : 0);
  if (!window) {
    LogError("SDL_CreateWindow failed");
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(gpuDevice, window)) {
    LogError("SDL_ClaimWindowForGPUDevice failed");
    return SDL_APP_FAILURE;
  }

  SDL_GPUShader *vertShader = LoadShader(gpuDevice, "triangle.vert.hlsl", "./shaders", 0, 1, 1, 0);
  SDL_GPUShader *fragShader = LoadShader(gpuDevice, "triangle.frag.hlsl", "./shaders", 0, 0, 0, 0);

  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
    .vertex_shader = vertShader,
    .fragment_shader = fragShader,
    .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .target_info = {
      .color_target_descriptions = (SDL_GPUColorTargetDescription[]){
        {.format = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window)}
      },
      .num_color_targets = 1,
    }
  };
  pipeline = SDL_CreateGPUGraphicsPipeline(gpuDevice, &pipelineCreateInfo);

  SDL_ReleaseGPUShader(gpuDevice, vertShader);
  SDL_ReleaseGPUShader(gpuDevice, fragShader);

  spriteBuffers = new GPUBufferPair(sizeof(SpriteInstance) * SPRITE_COUNT, gpuDevice);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  // Get current time
  Uint64 currentTime = SDL_GetPerformanceCounter();
  Uint64 frequency = SDL_GetPerformanceFrequency();

  // Initialize lastFPSUpdate on first frame
  if (lastFPSUpdate == 0) {
    lastFPSUpdate = currentTime;
  }

  // Calculate delta time
  double deltaTime = (double) (currentTime - lastFPSUpdate) / frequency;
  accumulatedTime += deltaTime;
  frameCount++;

  // Update FPS every second
  if (accumulatedTime >= 1.0) {
    double fps = frameCount / accumulatedTime;

    // Update window title
    char title[256];
    SDL_snprintf(title, sizeof(title), "Hello Buffers - FPS: %.1f", fps);
    SDL_SetWindowTitle(window, title);

    // Reset counters
    frameCount = 0;
    accumulatedTime = 0.0;
  }

  lastFPSUpdate = currentTime;

  commandBuffer = SDL_AcquireGPUCommandBuffer(gpuDevice);
  if (!commandBuffer) {
    LogError("SDL_AcquireGPUCommandBuffer failed");
    return SDL_APP_FAILURE;
  }

  if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, window, &swapchainTexture, nullptr, nullptr)) {
    LogError("SDL_WaitAndAcquireGPUSwapchainTexture failed");
    return SDL_APP_FAILURE;
  }

  if (swapchainTexture) {
    SpriteInstance *dataPtr = (SpriteInstance *) SDL_MapGPUTransferBuffer(
      gpuDevice,
      spriteBuffers->_transferBuffer,
      true);

    const float gridSpacing = 1.0f;
    const float shapeScale = 4.0f;

    // Compute grid dimensions in pixels
    float gridWidth = (1) * gridSpacing;
    float gridHeight = (1) * gridSpacing;

    // Center offset
    float offsetX = (800.0f - gridWidth) * 0.5f;
    float offsetY = (600.0f - gridHeight) * 0.5f;

    for (int i = 0; i < SPRITE_COUNT; i++) {
      int row = i;
      int col = i;
      dataPtr[i].x = offsetX + col * gridSpacing;
      dataPtr[i].y = offsetY + row * gridSpacing;
      dataPtr[i].z = dataPtr[i].y;
      dataPtr[i].rotation = 10.0f; // rotate based on time
      dataPtr[i].scale = shapeScale;
      // float angle = time + (i * 0.1f);
      // dataPtr[i].x = 400 + SDL_cosf(angle) * 200;
      // dataPtr[i].y = 300 + SDL_sinf(angle) * 200;
      // dataPtr[i].rotation = time;
      // dataPtr[i].scale = 20.0f + SDL_sinf(i) * 10.0f;
      dataPtr[i].r = 1.0f;
      dataPtr[i].g = 1.0f;
      dataPtr[i].b = 1.0f;
      dataPtr[i].a = 1.0f;

      if (row == 0 || col == 0 || row == 1 || col == 1) {
        dataPtr[i].g = 0.0f;
        dataPtr[i].b = 0.0f;
      }
    }

    SDL_UnmapGPUTransferBuffer(gpuDevice, spriteBuffers->_transferBuffer);

    // Upload shape data
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);
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
    SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(
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

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  switch (event->type) {
    case SDL_EVENT_QUIT:
      return SDL_APP_SUCCESS;
      break;

    case SDL_EVENT_KEY_DOWN:
      switch (event->key.scancode) {
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

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  spriteBuffers->ReleaseBuffers();
  SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipeline);
  SDL_ReleaseWindowFromGPUDevice(gpuDevice, window);
  SDL_DestroyGPUDevice(gpuDevice);
  SDL_DestroyWindow(window);
}

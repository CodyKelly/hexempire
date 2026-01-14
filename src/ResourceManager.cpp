//
// Created by hoy on 11/19/25.
//

#define LogError(error) (SDL_LogError(SDL_LOG_CATEGORY_ERROR, error))

#include <stdexcept>
#include <vector>
#include <string>
#include <ranges>

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <tracy/Tracy.hpp>

#include "ResourceManager.h"


void ResourceManager::Init(const char* windowTitle, int width, int height, SDL_WindowFlags windowFlags)
{
    ZoneScoped;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_ShaderCross_Init();

    gpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true, NULL);
    if (!gpuDevice)
    {
        LogError("SDL_CreateGPUDevice failed");
        throw std::runtime_error("SDL_CreateGPUDevice failed");
    }

    window = SDL_CreateWindow(windowTitle, width, height, windowFlags);
    if (!window)
    {
        LogError("SDL_CreateWindow failed");
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    if (!SDL_ClaimWindowForGPUDevice(gpuDevice, window))
    {
        LogError("SDL_ClaimWindowForGPUDevice failed");
        throw std::runtime_error("SDL_ClaimWindowForGPUDevice failed");
    }

    // Acquire and submit a swapchain texture to initialize the swapchain format
    // This is needed because SDL_GetGPUSwapchainTextureFormat may return INVALID
    // on some backends (e.g., Vulkan on Linux) before the first acquire
    SDL_GPUCommandBuffer* initCmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    if (initCmdBuf)
    {
        SDL_GPUTexture* swapchainTex = nullptr;
        SDL_WaitAndAcquireGPUSwapchainTexture(initCmdBuf, window, &swapchainTex, nullptr, nullptr);
        SDL_SubmitGPUCommandBuffer(initCmdBuf);
    }

    pipelines = std::unordered_map<string, SDL_GPUGraphicsPipeline*>();
    buffers = std::unordered_map<string, SDL_GPUBuffer*>();
    transferBuffers = std::unordered_map<string, SDL_GPUTransferBuffer*>();
}

ResourceManager::~ResourceManager()
{
    for (const auto buffer : buffers | std::views::values)
    {
        SDL_ReleaseGPUBuffer(gpuDevice, buffer);
    }
    for (const auto buffer : transferBuffers | std::views::values)
    {
        SDL_ReleaseGPUTransferBuffer(gpuDevice, buffer);
    }
    for (const auto pipeline : pipelines | std::views::values)
    {
        SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipeline);
    }
    for (const auto sampler : samplers | std::views::values)
    {
        SDL_ReleaseGPUSampler(gpuDevice, sampler);
    }
    for (const auto texture : textures | std::views::values)
    {
        SDL_ReleaseGPUTexture(gpuDevice, texture);
    }
    SDL_ShaderCross_Quit();
    SDL_ReleaseWindowFromGPUDevice(gpuDevice, window);
    SDL_DestroyGPUDevice(gpuDevice);
    SDL_DestroyWindow(window);
}

SDL_GPUBuffer* ResourceManager::CreateBuffer(const string& name, const SDL_GPUBufferCreateInfo* createInfo)
{
    if (buffers.contains(name))
    {
        throw std::runtime_error("CreateBuffer: Buffer already exists");
    }

    buffers[name] = SDL_CreateGPUBuffer(gpuDevice, createInfo);

    return buffers[name];
}

SDL_GPUTransferBuffer* ResourceManager::CreateTransferBuffer(const string& name,
                                                             const SDL_GPUTransferBufferCreateInfo* createInfo)
{
    if (transferBuffers.contains(name))
    {
        throw std::runtime_error("CreateBuffer: Buffer already exists");
    }

    transferBuffers[name] = SDL_CreateGPUTransferBuffer(gpuDevice, createInfo);

    return transferBuffers[name];
}

SDL_GPUSampler* ResourceManager::CreateSampler(const string& name, const SDL_GPUSamplerCreateInfo* samplerInfo)
{
    if (samplers.contains(name))
    {
        throw std::runtime_error("CreateSampler: Sampler already exists");
    }

    samplers[name] = SDL_CreateGPUSampler(gpuDevice, samplerInfo);

    return samplers[name];
}


SDL_GPUGraphicsPipeline* ResourceManager::CreateGraphicsPipeline(
    const string& name,
    const ShaderInfo& vertexShaderInfo,
    const ShaderInfo& fragmentShaderInfo
)
{
    ZoneScoped;
    if (pipelines.contains(name))
    {
        throw std::runtime_error("Resource already exists");
    }

    SDL_GPUShader* vertShader = LoadShader(vertexShaderInfo);
    SDL_GPUShader* fragShader = LoadShader(fragmentShaderInfo);

    SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window);
    SDL_Log("Swapchain texture format: %d", swapchainFormat);

    // Fallback if swapchain format is invalid (can happen on some backends)
    if (swapchainFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        swapchainFormat = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        SDL_Log("Using fallback format: %d", swapchainFormat);
    }

    SDL_GPUColorTargetDescription colorTargetDesc = {
        .format = swapchainFormat,
        .blend_state = {
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .enable_blend = true,
        },
    };

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .vertex_shader = vertShader,
        .fragment_shader = fragShader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .target_info = {
            .color_target_descriptions = &colorTargetDesc,
            .num_color_targets = 1,
        }
    };
    pipelines[name] = SDL_CreateGPUGraphicsPipeline(gpuDevice, &pipelineCreateInfo);

    SDL_ReleaseGPUShader(gpuDevice, vertShader);
    SDL_ReleaseGPUShader(gpuDevice, fragShader);

    return pipelines[name];
}


SDL_GPUShader* ResourceManager::LoadShader(const ShaderInfo& info) const
{
    ZoneScoped;
    if (info.shaderPath == nullptr)
        throw std::runtime_error("LoadShader: Shader path error");

    // Auto-detect the shader stage from the file name for convenience
    SDL_GPUShaderStage stage;
    SDL_ShaderCross_ShaderStage crossStage;
    if (SDL_strstr(info.shaderPath, ".vert"))
    {
        stage = SDL_GPU_SHADERSTAGE_VERTEX;
        crossStage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
    }
    else if (SDL_strstr(info.shaderPath, ".frag"))
    {
        stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        crossStage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
    }
    else
    {
        SDL_Log("Invalid shader stage!");
        return nullptr;
    }

    // Load shader source with size validation
    size_t hlslSourceSize;
    void* hlslSource = SDL_LoadFile(info.shaderPath, &hlslSourceSize);
    if (!hlslSource)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "LoadShader: Failed to load shader from '%s' - %s", info.shaderPath,
                     SDL_GetError());
        return nullptr;
    }

    SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(gpuDevice);
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const char* entrypoint;

    if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
    }
    else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL)
    {
        format = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No supported shader format available");
        SDL_free(hlslSource);
        return nullptr;
    }

    SDL_ShaderCross_HLSL_Info hlslInfo = {
        .source = static_cast<const char*>(hlslSource),
        .entrypoint = "main",
        .include_dir = nullptr,
        .defines = nullptr,
        .shader_stage = crossStage,
    };

    size_t codeSize;
    void* code = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &codeSize);


    if (format == SDL_GPU_SHADERFORMAT_MSL)
    {
        const auto spirvInfo = SDL_ShaderCross_SPIRV_Info{
            static_cast<const unsigned char*>(code),
            codeSize,
            "main",
            crossStage,
        };
        const auto mslCode = SDL_ShaderCross_TranspileMSLFromSPIRV(&spirvInfo);
        SDL_free(code);
        code = mslCode;
        codeSize = SDL_strlen(static_cast<const char*>(code));
    }

    if (!code)
    {
        SDL_Log("Failed to compile HLSL: %s", SDL_GetError());
        SDL_free(hlslSource);
        return nullptr;
    }

    SDL_free(hlslSource);

    SDL_GPUShaderCreateInfo shaderInfo;
    shaderInfo.stage = stage;
    shaderInfo.format = format;
    shaderInfo.code = (const Uint8*)code;
    shaderInfo.code_size = codeSize;
    shaderInfo.entrypoint = entrypoint;
    shaderInfo.num_samplers = info.samplerCount;
    shaderInfo.num_uniform_buffers = info.uniformBufferCount;
    shaderInfo.num_storage_buffers = info.storageBufferCount;
    shaderInfo.num_storage_textures = info.storageTextureCount;
    SDL_GPUShader* shader = SDL_CreateGPUShader(gpuDevice, &shaderInfo);
    if (shader == nullptr)
    {
        SDL_Log("Failed to create shader!");
        SDL_free(code);
        return nullptr;
    }

    SDL_free(code);

    SDL_Log("Successfully created GPU shader for %s", info.shaderPath);
    return shader;
}

SDL_Surface* ResourceManager::LoadPNG(const char* path, int desiredChannels)
{
    SDL_PixelFormat format;
    SDL_Surface* result = SDL_LoadPNG(path);

    if (result == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load PNG: %s", SDL_GetError());
        return nullptr;
    }

    if (desiredChannels == 4) format = SDL_PIXELFORMAT_ABGR8888;
    else
    {
        SDL_assert(!"Unexpected desiredChannels");
        SDL_DestroySurface(result);
        return nullptr;
    }
    if (result->format != format)
    {
        SDL_Surface* next = SDL_ConvertSurface(result, format);
        SDL_DestroySurface(result);
        result = next;
    }

    return result;
}

SDL_GPUGraphicsPipeline* ResourceManager::GetGraphicsPipeline(const string& name)
{
    if (!pipelines.contains(name))
    {
        return nullptr;
    }
    return pipelines[name];
}

SDL_GPUTexture* ResourceManager::CreateTexture(const string& name, const char* pngPath)
{
    ZoneScoped;
    SDL_Surface* surface = LoadPNG(pngPath, 4);
    if (!surface)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load texture: %s", pngPath);
        return nullptr;
    }

    SDL_Log("Loaded texture: %dx%d, format=%d", surface->w, surface->h, surface->format);

    auto textureInfo = SDL_GPUTextureCreateInfo{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = static_cast<Uint32>(surface->w),
        .height = static_cast<Uint32>(surface->h),
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };

    SDL_GPUTexture* texture = SDL_CreateGPUTexture(gpuDevice, &textureInfo);
    if (!texture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create GPU texture: %s", SDL_GetError());
        SDL_DestroySurface(surface);
        return nullptr;
    }

    // Create temporary transfer buffer for upload
    SDL_GPUTransferBufferCreateInfo transferInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = static_cast<Uint32>(surface->w * surface->h * 4)
    };
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);

    // Map and copy data
    auto* dataPtr = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false));
    SDL_memcpy(dataPtr, surface->pixels, surface->w * surface->h * 4);
    SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);

    // Upload to GPU
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTextureTransferInfo texTransferInfo = {
        .transfer_buffer = transferBuffer,
        .offset = 0,
    };
    SDL_GPUTextureRegion texRegion = {
        .texture = texture,
        .w = static_cast<Uint32>(surface->w),
        .h = static_cast<Uint32>(surface->h),
        .d = 1
    };
    SDL_UploadToGPUTexture(copyPass, &texTransferInfo, &texRegion, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);

    // Cleanup
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);
    SDL_DestroySurface(surface);

    textures[name] = texture;
    return texture;
}

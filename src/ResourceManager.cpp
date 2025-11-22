//
// Created by hoy on 11/19/25.
//

#define LogError(error) (SDL_LogError(SDL_LOG_CATEGORY_ERROR, error))

#include <stdexcept>
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include "ResourceManager.h"


void ResourceManager::Init(const char* windowTitle, int width, int height, SDL_WindowFlags windowFlags)
{
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
}

SDL_GPUTransferBuffer* ResourceManager::AcquireTransferBuffer(SDL_GPUTransferBufferCreateInfo* createInfo)
{
    for (auto buffer : transferBuffers)
    {
        if (sizeof(buffer) == createInfo->size)
        {
            return buffer;
        }
    }
}

ResourceManager::~ResourceManager()
{
    for (const auto buffer : buffers)
    {
        SDL_ReleaseGPUBuffer(gpuDevice, buffer.second);
    }
    for (const auto buffer : transferBuffers)
    {
        SDL_ReleaseGPUTransferBuffer(gpuDevice, buffer);
    }
}

SDL_GPUBuffer* ResourceManager::CreateBuffer(const char* name, SDL_GPUBufferCreateInfo* createInfo)
{
    if (buffers[name])
    {
        throw std::runtime_error("Resource already exists");
    }

    buffers[name] = SDL_CreateGPUBuffer(gpuDevice, createInfo);

    return buffers[name];
}

SDL_GPUGraphicsPipeline* ResourceManager::CreateGraphicsPipeline(
    const char* name,
    const ShaderInfo& vertexShaderInfo,
    const ShaderInfo& fragmentShaderInfo
)
{
    if (pipelines[name])
    {
        throw std::runtime_error("Resource already exists");
    }

    SDL_GPUShader* vertShader = LoadShader(vertexShaderInfo);
    SDL_GPUShader* fragShader = LoadShader(fragmentShaderInfo);

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .vertex_shader = vertShader,
        .fragment_shader = fragShader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .target_info = {
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {
                {
                    .format = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window)
                }
            },
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

    SDL_Log("Successfully created GPU shader for %s", shaderPath);
    return shader;
}

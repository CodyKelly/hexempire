#ifndef ATLAS_RESOURCEMANAGER_H
#define ATLAS_RESOURCEMANAGER_H

#include <vector>
#include <unordered_map>
#include <string>
#include <SDL3/SDL.h>

using std::vector, std::unordered_map, std::string;

struct ShaderInfo
{
    const char* shaderPath;
    Uint32 samplerCount;
    Uint32 uniformBufferCount;
    Uint32 storageBufferCount;
    Uint32 storageTextureCount;
};

class ResourceManager
{
private:
    SDL_Window* window = nullptr;
    SDL_GPUDevice* gpuDevice = nullptr;
    unordered_map<string, SDL_GPUGraphicsPipeline*> pipelines;
    unordered_map<string, SDL_GPUBuffer*> buffers;
    unordered_map<string, SDL_GPUTransferBuffer*> transferBuffers;
    unordered_map<string, SDL_GPUSampler*> samplers;
    unordered_map<string, SDL_GPUTexture*> textures;

public:
    void Init(const char* windowTitle, int width, int height, SDL_WindowFlags windowFlags);
    SDL_GPUBuffer* CreateBuffer(const string& name, const SDL_GPUBufferCreateInfo* createInfo);
    SDL_GPUTransferBuffer* CreateTransferBuffer(const string& name, const SDL_GPUTransferBufferCreateInfo* createInfo);
    SDL_GPUGraphicsPipeline* CreateGraphicsPipeline(
        const string& name,
        const ShaderInfo& vertexShaderInfo,
        const ShaderInfo& fragmentShaderInfo
    );
    SDL_GPUSampler* CreateSampler(const string& name, const SDL_GPUSamplerCreateInfo* samplerInfo);
    SDL_GPUTexture* CreateTexture(const string& name, const char* pngPath);
    static SDL_Surface* LoadPNG(const char* path, int desiredChannels);
    SDL_GPUShader* LoadShader(const ShaderInfo& info) const;
    SDL_GPUDevice* GetGPUDevice() const { return gpuDevice; }
    SDL_Window* GetWindow() const { return window; }
    SDL_GPUGraphicsPipeline* GetGraphicsPipeline(const string& name);
    ~ResourceManager();
};


#endif //ATLAS_RESOURCEMANAGER_H

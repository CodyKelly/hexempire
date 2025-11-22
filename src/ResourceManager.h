#ifndef ATLAS_RESOURCEMANAGER_H
#define ATLAS_RESOURCEMANAGER_H

#include <vector>
#include <unordered_map>
#include <SDL3/SDL.h>

using std::vector, std::unordered_map;

struct ShaderInfo
{
    const char* shaderPath;
    Uint32 samplerCount;
    Uint32 uniformBufferCount;
    Uint32 storageBufferCount;
    Uint32 storageTextureCount;
};

//
// class GPUBufferPair {
// public:
//     SDL_GPUTransferBuffer *_transferBuffer;
//     SDL_GPUBuffer *_dataBuffer;
//     SDL_GPUDevice *_gpuDevice;
//
//     GPUBufferPair(Uint32 size, SDL_GPUDevice *gpuDevice) : _gpuDevice(gpuDevice) {
//         SDL_GPUTransferBufferCreateInfo shapeTransferBufferCreateInfo = {
//             .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
//             .size = size,
//         };
//         _transferBuffer = SDL_CreateGPUTransferBuffer(_gpuDevice, &shapeTransferBufferCreateInfo);
//
//         SDL_GPUBufferCreateInfo shapeBufferCreateInfo = {
//             .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
//             .size = size,
//         };
//         _dataBuffer = SDL_CreateGPUBuffer(_gpuDevice, &shapeBufferCreateInfo);
//     }
//
//     ~GPUBufferPair() {
//         ReleaseBuffers();
//     }
//
//     void ReleaseBuffers() {
//         SDL_ReleaseGPUBuffer(_gpuDevice, _dataBuffer);
//         SDL_ReleaseGPUTransferBuffer(_gpuDevice, _transferBuffer);
//     }
// };

class ResourceManager
{
private:
    SDL_Window* window;
    SDL_GPUDevice* gpuDevice;
    unordered_map<const char*, SDL_GPUGraphicsPipeline*> pipelines;
    unordered_map<const char*, SDL_GPUBuffer*> buffers;
    vector<SDL_GPUTransferBuffer*> transferBuffers;

public:
    void Init(const char* windowTitle, int width, int height, SDL_WindowFlags windowFlags);
    SDL_GPUBuffer* CreateBuffer(const char* name, SDL_GPUBufferCreateInfo* createInfo);
    SDL_GPUTransferBuffer* AcquireTransferBuffer(SDL_GPUTransferBufferCreateInfo* createInfo);
    SDL_GPUGraphicsPipeline* CreateGraphicsPipeline(
        const char* name,
        const ShaderInfo& vertexShaderInfo,
        const ShaderInfo& fragmentShaderInfo
    );
    SDL_GPUShader* LoadShader(const ShaderInfo& info) const;
    ~ResourceManager();
};


#endif //ATLAS_RESOURCEMANAGER_H

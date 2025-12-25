//
// HexMapRenderer.cpp - Renders hex map implementation
//

#include "HexMapRenderer.h"
#include <cstring>

HexMapRenderer::HexMapRenderer(ResourceManager* rm)
    : _resourceManager(rm)
{
}

void HexMapRenderer::Initialize(size_t maxTileCount)
{
    _maxTileCount = maxTileCount;

    // Create GPU buffer for tile data
    SDL_GPUBufferCreateInfo bufferInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        .size = static_cast<Uint32>(sizeof(HexTileGPU) * _maxTileCount)
    };
    _tileBuffer = _resourceManager->CreateBuffer("hexTiles", &bufferInfo);

    // Create transfer buffer
    SDL_GPUTransferBufferCreateInfo transferInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = static_cast<Uint32>(sizeof(HexTileGPU) * _maxTileCount)
    };
    _transferBuffer = _resourceManager->CreateTransferBuffer("hexTilesTransfer", &transferInfo);

    _isDirty = true;
}

void HexMapRenderer::Upload(SDL_GPUCommandBuffer* commandBuffer)
{
    if (!_hexMapData) return;
    if (!_hexMapData->IsDirty() && !_isDirty) return;
    if (_hexMapData->GetTiles().empty()) return;

    // Map transfer buffer
    auto* dataPtr = static_cast<HexTileGPU*>(SDL_MapGPUTransferBuffer(
        _resourceManager->GetGPUDevice(),
        _transferBuffer,
        false
    ));

    if (!dataPtr) return;

    // Copy tile data
    size_t tileCount = std::min(_hexMapData->GetTileCount(), _maxTileCount);
    std::memcpy(dataPtr, _hexMapData->GetTiles().data(), sizeof(HexTileGPU) * tileCount);

    SDL_UnmapGPUTransferBuffer(_resourceManager->GetGPUDevice(), _transferBuffer);

    // Upload to GPU
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);

    SDL_GPUTransferBufferLocation transferLoc = {
        .transfer_buffer = _transferBuffer,
        .offset = 0
    };

    SDL_GPUBufferRegion bufferRegion = {
        .buffer = _tileBuffer,
        .offset = 0,
        .size = static_cast<Uint32>(sizeof(HexTileGPU) * tileCount)
    };

    SDL_UploadToGPUBuffer(copyPass, &transferLoc, &bufferRegion, true);
    SDL_EndGPUCopyPass(copyPass);

    _hexMapData->ClearDirty();
    _isDirty = false;
}

void HexMapRenderer::Draw(SDL_GPURenderPass* renderPass)
{
    if (!_hexMapData || _hexMapData->GetTiles().empty()) return;

    // Bind hex pipeline
    SDL_GPUGraphicsPipeline* pipeline = _resourceManager->GetGraphicsPipeline("hexTiles");
    if (!pipeline) return;

    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

    // Bind tile buffer as vertex storage
    SDL_BindGPUVertexStorageBuffers(renderPass, 0, &_tileBuffer, 1);

    // Draw: 18 vertices per hex (6 triangles = 18 verts for the hexagon)
    Uint32 vertexCount = static_cast<Uint32>(_hexMapData->GetTileCount()) * 18;
    SDL_DrawGPUPrimitives(renderPass, vertexCount, 1, 0, 0);
}

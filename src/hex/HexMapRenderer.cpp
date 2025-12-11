//
// Created by hoy on 12/10/25.
//

#include "HexMapRenderer.h"

void HexMapRenderer::Upload(SDL_GPUCommandBuffer* commandBuffer)
{
    if (!isDirty || _hexMapData->GetTiles().empty()) return;

    auto* dataPtr = static_cast<HexTile*>(SDL_MapGPUTransferBuffer(
        _resourceManager->GetGPUDevice(),
        _transferBuffer,
        false
    ));

    memcpy(dataPtr, _hexMapData->GetTiles().data(), sizeof(HexTile) * _hexMapData->GetTiles().size());

    SDL_UnmapGPUTransferBuffer(_resourceManager->GetGPUDevice(), _transferBuffer);

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    const SDL_GPUTransferBufferLocation transferLoc = {
        .transfer_buffer = _transferBuffer,
        .offset = 0
    };
    const SDL_GPUBufferRegion bufferRegion = {
        .buffer = _tileBuffer,
        .offset = 0,
        .size = static_cast<Uint32>(sizeof(HexTile) * _hexMapData->GetTiles().size())
    };
    SDL_UploadToGPUBuffer(copyPass, &transferLoc, &bufferRegion, true);
    SDL_EndGPUCopyPass(copyPass);

    isDirty = false;
}

void HexMapRenderer::Draw(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* commandBuffer,
                          const Matrix4x4& viewProjection)
{
    if (_hexMapData->GetTiles().empty()) return;

    SDL_BindGPUGraphicsPipeline(renderPass,
                                _resourceManager->GetGraphicsPipeline("sprites"));

    SDL_BindGPUVertexStorageBuffers(renderPass, 0, &_tileBuffer, 1);

    SDL_PushGPUVertexUniformData(commandBuffer, 0,
                                 &viewProjection, sizeof(Matrix4x4));

    SDL_DrawGPUPrimitives(renderPass, _hexMapData->GetTiles().size() * 6, 1, 0, 0);
}

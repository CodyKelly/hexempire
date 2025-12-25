//
// HexMapRenderer.h - Renders hex map to GPU
//

#ifndef ATLAS_HEXMAPRENDERER_H
#define ATLAS_HEXMAPRENDERER_H

#include "HexMapData.h"
#include "../ResourceManager.h"

class HexMapRenderer
{
public:
    HexMapRenderer(ResourceManager* rm);

    // Initialize GPU resources
    void Initialize(size_t maxTileCount);

    // Set the hex map data to render
    void SetHexMapData(HexMapData* data) { _hexMapData = data; }

    // Upload tile data to GPU if dirty
    void Upload(SDL_GPUCommandBuffer* commandBuffer);

    // Draw all hex tiles
    void Draw(SDL_GPURenderPass* renderPass);

    // Mark as needing reupload
    void MarkDirty() { _isDirty = true; }

private:
    ResourceManager* _resourceManager;
    HexMapData* _hexMapData = nullptr;
    SDL_GPUBuffer* _tileBuffer = nullptr;
    SDL_GPUTransferBuffer* _transferBuffer = nullptr;
    size_t _maxTileCount = 0;
    bool _isDirty = true;
};

#endif // ATLAS_HEXMAPRENDERER_H

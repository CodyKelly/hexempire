//
// Created by hoy on 12/10/25.
//

#ifndef ATLAS_HEXMAPRENDERER_H
#define ATLAS_HEXMAPRENDERER_H
#include "HexMapData.h"
#include "../Camera.h"
#include "../ResourceManager.h"


class HexMapRenderer
{
    ResourceManager* _resourceManager;
    HexMapData* _hexMapData;
    SDL_GPUBuffer* _tileBuffer;
    SDL_GPUTransferBuffer* _transferBuffer;
    SDL_GPUGraphicsPipeline* _pipeline;

    bool isDirty = true;

public:
    HexMapRenderer(ResourceManager*, HexMapData*);

    void Upload(SDL_GPUCommandBuffer*);
    void Draw(SDL_GPURenderPass*, SDL_GPUCommandBuffer*, const Matrix4x4&);
};


#endif //ATLAS_HEXMAPRENDERER_H

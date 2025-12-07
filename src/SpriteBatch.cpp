//
// Created by hoy on 11/21/25.
//

#include "SpriteBatch.h"

SpriteBatch::SpriteBatch(string batchName, ResourceManager* rm, Uint32 maxSpriteCount)
    : resourceManager(rm), maxSprites(maxSpriteCount), isDirty(true)
{
    name = batchName;

    SDL_GPUBufferCreateInfo bufferInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        .size = static_cast<Uint32>(sizeof(SpriteInstance)) * maxSprites
    };
    spriteBuffer = rm->CreateBuffer(name + "SpriteBatch", &bufferInfo);

    SDL_GPUTransferBufferCreateInfo transferInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = static_cast<Uint32>(sizeof(SpriteInstance)) * maxSprites
    };
    transferBuffer = rm->CreateTransferBuffer(name + "BatchTBuffer", &transferInfo);

    sprites.reserve(maxSprites);
}

void SpriteBatch::Reserve(size_t count)
{
    sprites.reserve(count);
}

void SpriteBatch::AddSprite(float x, float y, float scale)
{
    if (sprites.size() >= maxSprites)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "SpriteBatch full!");
        return;
    }

    SpriteInstance instance = {
        x, y, 0, 0,
        scale, 0, 0, 0,
        1, 1, 1, 1
    };
    sprites.push_back(instance);
    isDirty = true;
}

SpriteInstance* SpriteBatch::GetSpriteData()
{
    return sprites.data();
}

const SpriteInstance* SpriteBatch::GetSpriteData() const
{
    return sprites.data();
}

SpriteInstance& SpriteBatch::GetSprite(size_t index)
{
    return sprites[index];
}

const SpriteInstance& SpriteBatch::GetSprite(size_t index) const
{
    return sprites[index];
}

size_t SpriteBatch::GetSpriteCount() const
{
    return sprites.size();
}

void SpriteBatch::Clear()
{
    sprites.clear();
    isDirty = true;
}

void SpriteBatch::MarkDirty()
{
    isDirty = true;
}

void SpriteBatch::Upload(SDL_GPUCommandBuffer* commandBuffer)
{
    if (!isDirty || sprites.empty()) return;

    auto* dataPtr = static_cast<SpriteInstance*>(SDL_MapGPUTransferBuffer(
        resourceManager->GetGPUDevice(),
        transferBuffer,
        false
    ));

    memcpy(dataPtr, sprites.data(), sizeof(SpriteInstance) * sprites.size());

    SDL_UnmapGPUTransferBuffer(resourceManager->GetGPUDevice(), transferBuffer);

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    const SDL_GPUTransferBufferLocation transferLoc = {
        .transfer_buffer = transferBuffer,
        .offset = 0
    };
    const SDL_GPUBufferRegion bufferRegion = {
        .buffer = spriteBuffer,
        .offset = 0,
        .size = static_cast<Uint32>(sizeof(SpriteInstance) * sprites.size())
    };
    SDL_UploadToGPUBuffer(copyPass, &transferLoc, &bufferRegion, true);
    SDL_EndGPUCopyPass(copyPass);

    isDirty = false;
}

void SpriteBatch::Draw(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* commandBuffer,
                       const Matrix4x4& viewProjection)
{
    if (sprites.empty()) return;

    SDL_BindGPUGraphicsPipeline(renderPass,
                                resourceManager->GetGraphicsPipeline("sprites"));

    SDL_BindGPUVertexStorageBuffers(renderPass, 0, &spriteBuffer, 1);

    SDL_PushGPUVertexUniformData(commandBuffer, 0,
                                 &viewProjection, sizeof(Matrix4x4));

    SDL_DrawGPUPrimitives(renderPass, sprites.size() * 6, 1, 0, 0);
}

//
// Created by hoy on 11/21/25.
//

#include "SpriteBatch.h"

#include <stdexcept>

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

void SpriteBatch::AddSprite(const SpriteInstance& sprite)
{
    if (sprites.size() >= maxSprites)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "SpriteBatch full!");
        return;
    }

    sprites.push_back(sprite);
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
    if (index >= sprites.size()) throw std::out_of_range("index out of range");
    return sprites[index];
}

const SpriteInstance& SpriteBatch::GetSprite(size_t index) const
{
    if (index >= sprites.size()) throw std::out_of_range("index out of range");
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

    SDL_memcpy(dataPtr, sprites.data(), sizeof(SpriteInstance) * sprites.size());

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

void SpriteBatch::SetTexture(SDL_GPUTexture* tex, SDL_GPUSampler* samp)
{
    texture = tex;
    sampler = samp;
}

void SpriteBatch::Draw(SDL_GPURenderPass* renderPass)
{
    if (sprites.empty()) return;

    SDL_BindGPUGraphicsPipeline(renderPass,
                                resourceManager->GetGraphicsPipeline("sprites"));

    SDL_BindGPUVertexStorageBuffers(renderPass, 0, &spriteBuffer, 1);

    SDL_GPUTextureSamplerBinding textureSamplerBinding = {
        .texture = texture,
        .sampler = sampler
    };
    SDL_BindGPUFragmentSamplers(renderPass, 0, &textureSamplerBinding, 1);

    SDL_DrawGPUPrimitives(renderPass, sprites.size() * 6, 1, 0, 0);
}

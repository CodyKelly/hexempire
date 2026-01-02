//
// DiceRenderer.cpp - Dice rendering implementation
//

#include "DiceRenderer.h"

#include <tracy/Tracy.hpp>

DiceRenderer::DiceRenderer(ResourceManager *rm)
    : _resourceManager(rm) {
}

void DiceRenderer::Initialize(size_t maxDice, SDL_GPUTexture *texture, SDL_GPUSampler *sampler) {
    _spriteBatch = new SpriteBatch("dice", _resourceManager, static_cast<Uint32>(maxDice));

    if (texture && sampler) {
        _spriteBatch->SetTexture(texture, sampler);
    }
}

void DiceRenderer::UpdateFromGameState(
    const GameState &state,
    const HexGrid &grid) {
    ZoneScoped;
    if (!_spriteBatch) return;

    _spriteBatch->Clear();

    // Add dice for each territory
    for (const TerritoryData &territory: state.territories) {
        if (territory.owner == PLAYER_NONE) continue;
        if (territory.diceCount == 0) continue;

        // Get world position of territory center
        Vector2 worldPos = grid.HexToWorld(territory.centerHex);

        // Get player for color
        const PlayerData &player = state.GetPlayer(territory.owner);

        AddDiceStack(worldPos, territory.diceCount, player);
    }

    _spriteBatch->MarkDirty();
}

void DiceRenderer::AddDiceStack(
    const Vector2 &worldPos,
    int diceCount,
    const PlayerData &owner) {
    // Stack dice vertically with slight offset
    // Start from bottom, go up
    float startY = worldPos.y + (diceCount - 1) * DICE_STACK_OFFSET * 0.5f;

    for (int i = 0; i < diceCount; i++) {
        SpriteInstance sprite{};

        // Position: center on territory, stack upward
        sprite.x = worldPos.x - DICE_SIZE * 0.5f;
        sprite.y = startY - i * DICE_STACK_OFFSET;
        sprite.z = 0.1f + i * DICE_Z_OFFSET; // Depth for proper ordering

        sprite.rotation = 0.0f;

        // Size
        sprite.w = DICE_SIZE;
        sprite.h = DICE_SIZE;

        // Texture coordinates for dice face
        // Using a simple white square from atlas (0,0) to represent dice
        // In a full implementation, you'd have actual dice face sprites
        sprite.tex_u = 0.0f;
        sprite.tex_v = 0.0f;
        sprite.tex_w = 1.0f;
        sprite.tex_h = 1.0f;

        // Use player color with some brightness variation for depth
        float brightness = 1.0f - i * 0.05f; // Slightly darker as stack goes down
        sprite.r = owner.colorR * brightness;
        sprite.g = owner.colorG * brightness;
        sprite.b = owner.colorB * brightness;
        sprite.a = 1.0f;

        _spriteBatch->AddSprite(sprite);
    }
}

void DiceRenderer::Upload(SDL_GPUCommandBuffer *cmd) {
    ZoneScoped;
    if (_spriteBatch) {
        _spriteBatch->Upload(cmd);
    }
}

void DiceRenderer::Draw(SDL_GPURenderPass *pass) {
    ZoneScoped;
    if (_spriteBatch) {
        _spriteBatch->Draw(pass);
    }
}

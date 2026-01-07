//
// DiceRenderer.h - Renders dice stacks on territories
//

#ifndef ATLAS_DICERENDERER_H
#define ATLAS_DICERENDERER_H

#include "../SpriteBatch.h"
#include "../game/GameData.h"
#include "../hex/HexGrid.h"

class DiceRenderer {
public:
    DiceRenderer(ResourceManager *rm);

    // Initialize (create sprite batch)
    void Initialize(size_t maxDice, SDL_GPUTexture *texture, SDL_GPUSampler *sampler);

    // Update sprites from game state
    void UpdateFromGameState(
        const GameState &state,
        const HexGrid &grid
    );

    // Upload to GPU
    void Upload(SDL_GPUCommandBuffer *cmd);

    // Draw dice
    void Draw(SDL_GPURenderPass *pass);

private:
    ResourceManager *_resourceManager;
    SpriteBatch *_spriteBatch = nullptr;

    // Dice visual parameters
    static constexpr float DICE_SIZE = 36.0f;
    static constexpr float DICE_STACK_OFFSET = 20.0f; // Vertical offset between stacked dice
    static constexpr float DICE_Z_OFFSET = 0.01f; // Depth offset per die
    static constexpr float DICE_STACK_MARGIN = 15.0f; // Space around each dice stack
    static constexpr uint32_t MAX_DICE_PER_STACK = 8;

    // Add dice stack sprites for a territory
    void AddDiceStack(
        const Vector2 &worldPos,
        int diceCount,
        const PlayerData &owner
    );
};

#endif // ATLAS_DICERENDERER_H

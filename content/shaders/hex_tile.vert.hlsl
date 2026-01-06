// Hex tile vertex shader with position, color, and highlight support

struct HexTileData
{
    float2 Position;      // World position of hex center
    float HexSize;        // Outer radius
    uint Flags;           // Selection/hover/target/border flags
    float4 Color;         // Territory color
    float4 Highlight;     // Highlight overlay color
};

struct Output
{
    float4 Color : TEXCOORD0;
    float4 Highlight : TEXCOORD1;
    float EdgeDist : TEXCOORD2;     // Distance from center (0-1)
    float BorderType : TEXCOORD3;   // 0=none, 1=same-owner territory, 2=enemy border
    float4 Position : SV_Position;
};

StructuredBuffer<HexTileData> DataBuffer : register(t0, space0);

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 ViewProjectionMatrix : packoffset(c0);
    float Time : packoffset(c4.x);   // For animations
};

// Pointy-top hexagon vertices (center + 6 corners)
static const float2 hexVertices[7] = {
    {0.0f, 0.0f},                    // Center
    {0.0f, 1.0f},                    // Top
    {0.866025404f, 0.5f},            // Top-right
    {0.866025404f, -0.5f},           // Bottom-right
    {0.0f, -1.0f},                   // Bottom
    {-0.866025404f, -0.5f},          // Bottom-left
    {-0.866025404f, 0.5f}            // Top-left
};

// 6 triangles from center (18 vertices total)
static const int triangleIndices[18] = {
    0, 1, 2,    // Triangle 0: center, top, top-right       -> NE edge
    0, 2, 3,    // Triangle 1: center, top-right, bottom-right -> E edge
    0, 3, 4,    // Triangle 2: center, bottom-right, bottom -> SE edge
    0, 4, 5,    // Triangle 3: center, bottom, bottom-left  -> SW edge
    0, 5, 6,    // Triangle 4: center, bottom-left, top-left -> W edge
    0, 6, 1     // Triangle 5: center, top-left, top        -> NW edge
};

// Map triangle index to HEX_DIRECTIONS index (East=0, NE=1, NW=2, West=3, SW=4, SE=5)
// Note: Y is flipped between shader coords and screen, so top/bottom triangles are swapped
static const int triangleToDirection[6] = { 5, 0, 1, 2, 3, 4 };

Output main(uint id : SV_VertexID)
{
    uint tileIndex = id / 18;
    uint localVertId = id % 18;
    uint triangleIndex = localVertId / 3;
    uint vertIndex = triangleIndices[localVertId];

    HexTileData tile = DataBuffer[tileIndex];

    // Scale vertex by hex size and offset by position
    float2 localPos = hexVertices[vertIndex] * tile.HexSize;
    float2 worldPos = localPos + tile.Position;

    // Calculate edge distance (0 at center, 1 at edge)
    float edgeDist = length(hexVertices[vertIndex]);

    // Determine border type for this triangle's edge
    int direction = triangleToDirection[triangleIndex];
    uint borderBit = (tile.Flags >> (4 + direction)) & 1;   // Territory border
    uint enemyBit = (tile.Flags >> (10 + direction)) & 1;   // Enemy border

    float borderType = 0.0f;
    if (enemyBit > 0)
        borderType = 2.0f;  // Enemy border (more pronounced)
    else if (borderBit > 0)
        borderType = 1.0f;  // Same-owner territory border

    Output output;
    output.Position = mul(ViewProjectionMatrix, float4(worldPos, 0.0f, 1.0f));
    output.Color = tile.Color;
    output.Highlight = tile.Highlight;
    output.EdgeDist = edgeDist;
    output.BorderType = borderType;

    return output;
}

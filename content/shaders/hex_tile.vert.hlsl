// Hex tile vertex shader with position, color, and highlight support

struct HexTileData
{
    float2 Position;      // World position of hex center
    float HexSize;        // Outer radius
    uint Flags;           // Selection/hover/target flags
    float4 Color;         // Territory color
    float4 Highlight;     // Highlight overlay color
};

struct Output
{
    float4 Color : TEXCOORD0;
    float4 Highlight : TEXCOORD1;
    float EdgeDist : TEXCOORD2;  // For border rendering
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
    0, 1, 2,    // Triangle 0: center, top, top-right
    0, 2, 3,    // Triangle 1: center, top-right, bottom-right
    0, 3, 4,    // Triangle 2: center, bottom-right, bottom
    0, 4, 5,    // Triangle 3: center, bottom, bottom-left
    0, 5, 6,    // Triangle 4: center, bottom-left, top-left
    0, 6, 1     // Triangle 5: center, top-left, top
};

Output main(uint id : SV_VertexID)
{
    uint tileIndex = id / 18;
    uint vertIndex = triangleIndices[id % 18];

    HexTileData tile = DataBuffer[tileIndex];

    // Scale vertex by hex size and offset by position
    float2 localPos = hexVertices[vertIndex] * tile.HexSize;
    float2 worldPos = localPos + tile.Position;

    // Calculate edge distance (0 at center, 1 at edge)
    float edgeDist = length(hexVertices[vertIndex]);

    Output output;
    output.Position = mul(ViewProjectionMatrix, float4(worldPos, 0.0f, 1.0f));
    output.Color = tile.Color;
    output.Highlight = tile.Highlight;
    output.EdgeDist = edgeDist;

    return output;
}

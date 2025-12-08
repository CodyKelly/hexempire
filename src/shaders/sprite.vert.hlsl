struct SpriteData
{
    float3 Position;
    float Rotation;
    float2 Scale;
    float2 Padding;
    float TexU, TexV, TexW, TexH;
    float4 Color;
};

struct Output
{
    float2 Texcoord: TEXCOORD0;
    float4 Color : TEXCOORD1;
    float4 Position : SV_Position;
};

StructuredBuffer<SpriteData> DataBuffer : register(t0, space0);

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 ViewProjectionMatrix : packoffset(c0);
};

static const float2 vertexPos[6] = {
    { 0.0f, 0.0f },  // Top-left
    { 1.0f, 0.0f },  // Top-right
    { 0.0f, 1.0f },  // Bottom-left

    { 1.0f, 0.0f },  // Top-right
    { 1.0f, 1.0f },  // Bottom-right
    { 0.0f, 1.0f }   // Bottom-left
};

Output main(uint id: SV_VertexID)
{
    uint shapeIndex = id / 6;
    uint vert = id % 6;
    SpriteData shape = DataBuffer[shapeIndex];

    float c = cos(shape.Rotation);
    float s = sin(shape.Rotation);

    float2 coord = vertexPos[vert];

    float2 texCoord = coord;
    texCoord.x = shape.TexU + texCoord.x * shape.TexW;
    texCoord.y = shape.TexV + texCoord.y * shape.TexH;

    coord *= shape.Scale;
    float2x2 rotation = { c, s, -s, c };
    coord = mul(coord, rotation);

    float3 coordWithDepth = float3(coord + shape.Position.xy, shape.Position.z);

    Output output;

    output.Position = mul(ViewProjectionMatrix, float4(coordWithDepth, 1.0f));
	output.Texcoord = texCoord;
    output.Color = shape.Color;
    return output;
}

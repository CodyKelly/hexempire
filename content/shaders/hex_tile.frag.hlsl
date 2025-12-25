// Hex tile fragment shader with border and highlight effects

struct Input
{
    float4 Color : TEXCOORD0;
    float4 Highlight : TEXCOORD1;
    float EdgeDist : TEXCOORD2;
};

float4 main(Input input) : SV_Target0
{
    float4 baseColor = input.Color;

    // Add border darkening at edges
    float borderWidth = 0.15f;
    float borderStart = 1.0f - borderWidth;

    if (input.EdgeDist > borderStart)
    {
        float borderFactor = (input.EdgeDist - borderStart) / borderWidth;
        baseColor.rgb *= lerp(1.0f, 0.6f, borderFactor);
    }

    // Apply highlight overlay
    if (input.Highlight.a > 0.0f)
    {
        baseColor.rgb = lerp(baseColor.rgb, input.Highlight.rgb, input.Highlight.a);
    }

    return baseColor;
}

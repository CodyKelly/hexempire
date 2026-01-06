// Hex tile fragment shader with border and highlight effects

struct Input
{
    float4 Color : TEXCOORD0;
    float4 Highlight : TEXCOORD1;
    float EdgeDist : TEXCOORD2;
    float BorderType : TEXCOORD3;  // 0=none, 1=same-owner territory, 2=enemy border
};

float4 main(Input input) : SV_Target0
{
    float4 baseColor = input.Color;

    float borderWidth = 0.08f;   // default: faint same-territory
    float borderDarkness = 0.92f;

    if (input.BorderType > 1.5f) {
        // Enemy border - most pronounced
        borderWidth = 0.18f;
        borderDarkness = 0.4f;
    } else if (input.BorderType > 0.5f) {
        // Same-owner different territory
        borderWidth = 0.12f;
        borderDarkness = 0.7f;
    }

    // Always draw border (no if-check)
    float borderStart = 1.0f - borderWidth;
    if (input.EdgeDist > borderStart) {
        float borderFactor = (input.EdgeDist - borderStart) / borderWidth;
        baseColor.rgb *= lerp(1.0f, borderDarkness, borderFactor);
    }


    // Apply highlight overlay
    if (input.Highlight.a > 0.0f)
    {
        baseColor.rgb = lerp(baseColor.rgb, input.Highlight.rgb, input.Highlight.a);
    }

    return baseColor;
}

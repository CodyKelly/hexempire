Texture2D <float4> Texture : register(t0, space2);
SampleState Sampler : register(s0, space2);

float4 main(float4 TexCoord: TEXCOORD0) : SV_Target0
{
    return Texture.Sample(Sampler, TexCoord);
}

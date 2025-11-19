Texture2D tex : register(t0);
SamplerState splr : register(s0);

float4 main(float2 tc : TexCoord) : SV_TARGET
{
    return tex.Sample(splr, tc);
}
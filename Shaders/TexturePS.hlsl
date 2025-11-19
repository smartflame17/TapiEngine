Texture2D tex : register(t0);
SamplerState splr : register(s0);

float4 main(float2 tex : TexCoord) : SV_Target
{
    return tex.Sample(splr, tex);
}
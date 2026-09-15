#include "Skinning.hlsli"
cbuffer CBuf : register(b0)
{
    matrix modelViewProj;
    matrix model;
    matrix normalModel;
};
struct VSOut
{
    float3 worldPos : Position;
    float3 normal : NORMAL;
    float4 pos : SV_POSITION;
};
VSOut main(float3 pos : POSITION, float3 n : NORMAL,
    uint4 indices : BLENDINDICES, float4 weights : BLENDWEIGHT)
{
    VSOut result;
    float4 skinned = SkinPosition(pos, indices, weights);
    result.worldPos = mul(skinned, model).xyz;
    result.pos = mul(skinned, modelViewProj);
    result.normal = normalize(mul(SkinNormal(n, indices, weights), (float3x3)normalModel));
    return result;
}

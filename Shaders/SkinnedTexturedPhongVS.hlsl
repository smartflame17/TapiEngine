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
    float3 tangent : TANGENT;
    float2 tex : TexCoord;
    float4 pos : SV_POSITION;
};
VSOut main(float3 pos : POSITION, float3 n : NORMAL, float2 tex : TEXCOORD,
    float3 tangent : TANGENT, uint4 indices : BLENDINDICES, float4 weights : BLENDWEIGHT)
{
    VSOut result;
    float4 skinned = SkinPosition(pos, indices, weights);
    result.worldPos = mul(skinned, model).xyz;
    result.pos = mul(skinned, modelViewProj);
    result.normal = normalize(mul(SkinNormal(n, indices, weights), (float3x3)normalModel));
    float3 t = mul(SkinTangent(tangent, indices, weights), (float3x3)model);
    result.tangent = normalize(t - result.normal * dot(t, result.normal));
    result.tex = tex;
    return result;
}

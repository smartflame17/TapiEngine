#ifndef SKINNING_INCLUDED
#define SKINNING_INCLUDED
#define MAX_SKIN_BONES 128
cbuffer SkinningCbuf : register(b1)
{
    matrix boneMatrices[MAX_SKIN_BONES];
    matrix boneNormalMatrices[MAX_SKIN_BONES];
};

float4 SkinPosition(float3 pos, uint4 indices, float4 weights)
{
    if (dot(weights, float4(1, 1, 1, 1)) == 0.0f) return float4(pos, 1.0f);
    float4 result = 0;
    [unroll] for (uint i = 0; i < 4; ++i)
        result += weights[i] * mul(float4(pos, 1.0f), boneMatrices[indices[i]]);
    return result;
}
float3 SkinNormal(float3 normal, uint4 indices, float4 weights)
{
    if (dot(weights, float4(1, 1, 1, 1)) == 0.0f) return normal;
    float3 result = 0;
    [unroll] for (uint i = 0; i < 4; ++i)
        result += weights[i] * mul(normal, (float3x3)boneNormalMatrices[indices[i]]);
    return result;
}
float3 SkinTangent(float3 tangent, uint4 indices, float4 weights)
{
    if (dot(weights, float4(1, 1, 1, 1)) == 0.0f) return tangent;
    float3 result = 0;
    [unroll] for (uint i = 0; i < 4; ++i)
        result += weights[i] * mul(tangent, (float3x3)boneMatrices[indices[i]]);
    return result;
}
#endif

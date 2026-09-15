#include "Skinning.hlsli"
cbuffer ShadowPassTransformCbuf : register(b0)
{
    matrix modelLightViewProjection;
};
float4 main(float3 pos : POSITION, uint4 indices : BLENDINDICES,
    float4 weights : BLENDWEIGHT) : SV_POSITION
{
    return mul(SkinPosition(pos, indices, weights), modelLightViewProjection);
}

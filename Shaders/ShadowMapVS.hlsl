cbuffer ShadowPassTransformCbuf : register(b0)
{
    matrix modelLightViewProjection;
};

float4 main(float3 pos : POSITION) : SV_POSITION
{
    return mul(float4(pos, 1.0f), modelLightViewProjection);
}

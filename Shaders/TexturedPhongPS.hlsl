cbuffer MaterialCbuf : register(b0)
{
    float3 materialColor;
    float specularIntensity;
    float specularPower;
    float3 specularColor;
    uint useNormalMap;
    uint materialPadding0;
    float2 materialPadding1;
};

cbuffer FrameLightCbuf : register(b1)
{
    float3 ambientColor;
    uint applyAmbient;
    uint hasActiveLight;
    uint lightType;
    float2 framePadding;
};

cbuffer CameraCbuf : register(b2)
{
    float3 cameraPos;
    float cameraPadding;
};

cbuffer LightPassCbuf : register(b3)
{
    float3 lightColor;
    float lightIntensity;
    float3 lightDirection;
    float lightAttConst;
    float3 lightPosition;
    float lightAttLinear;
    float lightAttQuad;
    uint lightEnabled;
    float2 lightPadding;
};

Texture2D baseColorTex : register(t0);
Texture2D normalMapTex : register(t1);
SamplerState splr : register(s0);

#include "LightingCommon.hlsli"

float4 main(float3 worldPos : POSITION, float3 n : NORMAL, float3 tangent : TANGENT, float2 texCoord : TEXCOORD) : SV_TARGET
{
    float3 N = normalize(n);
    const float3 T = normalize(tangent - dot(tangent, N) * N);
    const float3 B = normalize(cross(N, T));
    if (useNormalMap != 0u)
    {
        const float3 sampledNormal = normalMapTex.Sample(splr, texCoord).xyz * 2.0f - 1.0f;
        N = normalize(sampledNormal.x * T + sampledNormal.y * B + sampledNormal.z * N);
    }

    const float3 sampledColor = baseColorTex.Sample(splr, texCoord).rgb * materialColor;
    const float3 viewDir = normalize(cameraPos - worldPos);

    RenderLightData light;
    light.color = lightColor;
    light.intensity = lightIntensity;
    light.direction = lightDirection;
    light.attConst = lightAttConst;
    light.position = lightPosition;
    light.attLinear = lightAttLinear;
    light.attQuad = lightAttQuad;
    light.enabled = lightEnabled;

    const float3 ambient = applyAmbient != 0u ? sampledColor * ambientColor : 0.0f.xxx;
    const float3 lit = hasActiveLight != 0u
        ? EvaluatePhongLight(lightType, light, worldPos, N, viewDir, specularIntensity, specularPower, specularColor)
        : 0.0f.xxx;
    const float3 color = saturate(ambient + sampledColor * lit);

    return float4(color, 1.0f);
}

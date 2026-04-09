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
    float lightInnerConeCos;
    float lightOuterConeCos;
    uint lightEnabled;
    float3 lightPadding;
};

#include "LightingCommon.hlsli"

float4 main(float3 worldPos: POSITION, float3 n : NORMAL) : SV_TARGET
{
    const float3 N = normalize(n);
    const float3 viewDir = normalize(cameraPos - worldPos);

    RenderLightData light;
    light.color = lightColor;
    light.intensity = lightIntensity;
    light.direction = lightDirection;
    light.attConst = lightAttConst;
    light.position = lightPosition;
    light.attLinear = lightAttLinear;
    light.attQuad = lightAttQuad;
    light.innerConeCos = lightInnerConeCos;
    light.outerConeCos = lightOuterConeCos;
    light.enabled = lightEnabled;

    const float3 ambient = applyAmbient != 0u ? materialColor * ambientColor : 0.0f.xxx;
    const float3 lit = hasActiveLight != 0u
        ? EvaluatePhongLight(lightType, light, worldPos, N, viewDir, specularIntensity, specularPower, specularColor)
        : 0.0f.xxx;
    const float3 color = saturate(ambient + materialColor * lit);

    return float4(color, 1.0f);
}

struct RenderLightData
{
    float3 color;
    float intensity;
    float3 direction;
    float attConst;
    float3 position;
    float attLinear;
    float attQuad;
    float innerConeCos;
    float outerConeCos;
    uint enabled;
    //float3 padding;
};

Texture2D directionalShadowMapTex : register(t4);
Texture2D spotShadowMapTex : register(t2);
TextureCube pointShadowMapTex : register(t3);
SamplerState shadowSampler : register(s1);

cbuffer LightShadowCbuf : register(b4)
{
    matrix lightViewProjection[6];
    float3 shadowLightPosition;
    float shadowStrength;
    float2 shadowMapTexelSize;
    uint shadowEnabled;
    uint shadowType;
};

static const uint SHADOW_TYPE_NONE = 0u;
static const uint SHADOW_TYPE_SPOT = 1u;
static const uint SHADOW_TYPE_POINT = 2u;
static const uint SHADOW_TYPE_DIRECTIONAL = 3u;

uint GetPointShadowFaceIndex(float3 direction)
{
    uint faceIndex = 0u;
    const float3 absDirection = abs(direction);
    if (absDirection.x >= absDirection.y && absDirection.x >= absDirection.z)
    {
        faceIndex = direction.x >= 0.0f ? 0u : 1u;
    }
    else if (absDirection.y >= absDirection.z)
    {
        faceIndex = direction.y >= 0.0f ? 2u : 3u;
    }
    else
    {
        faceIndex = direction.z >= 0.0f ? 4u : 5u;
    }
    return faceIndex;
}

float SampleSpotShadowFactor(float3 worldPos, float3 normal, float3 lightDirection)
{
    if (shadowEnabled == 0u || shadowType != SHADOW_TYPE_SPOT)
    {
        return 1.0f;
    }

    float4 shadowClip = mul(float4(worldPos, 1.0f), lightViewProjection[0]);
    float3 shadowNdc = 0.0f.xxx;
    shadowNdc = shadowClip.xyz * (1.0f / max(shadowClip.w, 0.0001f));
    float2 shadowUv = 0.0f.xx;
    shadowUv = float2(shadowNdc.x * 0.5f + 0.5f, -shadowNdc.y * 0.5f + 0.5f);

    if (shadowUv.x < 0.0f || shadowUv.x > 1.0f || shadowUv.y < 0.0f || shadowUv.y > 1.0f || shadowNdc.z <= 0.0f || shadowNdc.z >= 1.0f)
    {
        return 1.0f;
    }

    const float depthBias = max(0.00035f * (1.0f - saturate(dot(normal, normalize(-lightDirection)))), 0.0001f);
    const float compareDepth = shadowNdc.z - depthBias;
    float visibility = 0.0f;

    visibility += compareDepth <= spotShadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(-0.5f, -0.5f)).r ? 1.0f : 0.0f;
    visibility += compareDepth <= spotShadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(0.5f, -0.5f)).r ? 1.0f : 0.0f;
    visibility += compareDepth <= spotShadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(-0.5f, 0.5f)).r ? 1.0f : 0.0f;
    visibility += compareDepth <= spotShadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(0.5f, 0.5f)).r ? 1.0f : 0.0f;
    visibility *= 0.25f;

    return lerp(1.0f - shadowStrength, 1.0f, visibility);
}

float SampleDirectionalShadowFactor(float3 worldPos, float3 normal, float3 lightDirection)
{
    if (shadowEnabled == 0u || shadowType != SHADOW_TYPE_DIRECTIONAL)
    {
        return 1.0f;
    }

    float4 shadowClip = mul(float4(worldPos, 1.0f), lightViewProjection[0]);
    float3 shadowNdc = 0.0f.xxx;
    shadowNdc = shadowClip.xyz;
    float2 shadowUv = 0.0f.xx;
    shadowUv = float2(shadowNdc.x * 0.5f + 0.5f, -shadowNdc.y * 0.5f + 0.5f);

    if (shadowUv.x < 0.0f || shadowUv.x > 1.0f || shadowUv.y < 0.0f || shadowUv.y > 1.0f || shadowNdc.z <= 0.0f || shadowNdc.z >= 1.0f)
    {
        return 1.0f;
    }

    const float depthBias = max(0.0003f * (1.0f - saturate(dot(normal, normalize(-lightDirection)))), 0.00008f);
    const float compareDepth = shadowNdc.z - depthBias;
    float visibility = 0.0f;

    visibility += compareDepth <= directionalShadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(-0.5f, -0.5f)).r ? 1.0f : 0.0f;
    visibility += compareDepth <= directionalShadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(0.5f, -0.5f)).r ? 1.0f : 0.0f;
    visibility += compareDepth <= directionalShadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(-0.5f, 0.5f)).r ? 1.0f : 0.0f;
    visibility += compareDepth <= directionalShadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(0.5f, 0.5f)).r ? 1.0f : 0.0f;
    visibility *= 0.25f;

    return lerp(1.0f - shadowStrength, 1.0f, visibility);
}

float SamplePointShadowFactor(float3 worldPos, float3 normal, float3 lightDirectionToLight)
{
    if (shadowEnabled == 0u || shadowType != SHADOW_TYPE_POINT)
    {
        return 1.0f;
    }

    const float3 toFragment = worldPos - shadowLightPosition;
    const float toFragmentLengthSq = dot(toFragment, toFragment);
    if (toFragmentLengthSq <= 0.000001f)
    {
        return 1.0f;
    }

    const uint faceIndex = GetPointShadowFaceIndex(toFragment);
    float4 shadowClip = mul(float4(worldPos, 1.0f), lightViewProjection[faceIndex]);
    float3 shadowNdc = 0.0f.xxx;
    shadowNdc = shadowClip.xyz * (1.0f / max(shadowClip.w, 0.0001f));
    if (shadowNdc.z <= 0.0f || shadowNdc.z >= 1.0f)
    {
        return 1.0f;
    }

    const float depthBias = max(0.0015f * (1.0f - saturate(dot(normal, lightDirectionToLight))), 0.0005f);
    const float compareDepth = shadowNdc.z - depthBias;
    const float visibility = compareDepth <= pointShadowMapTex.Sample(shadowSampler, normalize(toFragment)).r ? 1.0f : 0.0f;
    return lerp(1.0f - shadowStrength, 1.0f, visibility);
}

float3 EvaluatePhongLight(
    uint lightType,
    RenderLightData light,
    float3 worldPos,
    float3 normal,
    float3 viewDir,
    float specularIntensity,
    float specularPower,
    float3 specularColor)
{
    if (light.enabled == 0u || lightType == 0u)
    {
        return 0.0f.xxx;
    }

    float3 L = 0.0f.xxx;
    float attenuation = 1.0f;

    if (lightType == 1u) // directional light
    {
        L = normalize(-light.direction);
    }
    else if (lightType == 2u || lightType == 3u) // point/spot light
    {
        const float3 toLight = light.position - worldPos;
        const float distance = length(toLight);
        if (distance <= 0.0001f)
        {
            return 0.0f.xxx;
        }

        L = toLight / distance;
        attenuation = light.attConst + light.attLinear * distance + light.attQuad * distance * distance;

        if (lightType == 3u) // spot light
        {
            const float spotCos = dot(normalize(-light.direction), L);
            const float coneRange = max(light.innerConeCos - light.outerConeCos, 0.0001f);
            const float spotFactor = saturate((spotCos - light.outerConeCos) / coneRange);
            attenuation /= max(spotFactor, 0.0001f);
        }
    }
    else
    {
        return 0.0f.xxx;
    }

    const float NdotL = max(dot(normal, L), 0.0f);
    float shadowFactor = 1.0f;
    if (lightType == 1u)
    {
        shadowFactor = SampleDirectionalShadowFactor(worldPos, normal, light.direction);
    }
    else if (lightType == 3u)
    {
        shadowFactor = SampleSpotShadowFactor(worldPos, normal, light.direction);
    }
    else if (lightType == 2u)
    {
        shadowFactor = SamplePointShadowFactor(worldPos, normal, L);
    }
    const float3 diffuse = light.intensity * light.color * NdotL / max(attenuation, 0.0001f);

    const float3 reflectionDir = reflect(-L, normal);
    const float specularFactor = pow(max(dot(viewDir, reflectionDir), 0.0f), specularPower);
    const float3 specular = specularIntensity * specularColor * specularFactor * light.intensity / max(attenuation, 0.0001f);

    return (diffuse + specular) * shadowFactor;
}

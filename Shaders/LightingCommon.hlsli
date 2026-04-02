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

Texture2D shadowMapTex : register(t2);
SamplerState shadowSampler : register(s1);

cbuffer LightShadowCbuf : register(b4)
{
    matrix lightViewProjection;
    float2 shadowMapTexelSize;
    uint shadowEnabled;
    float shadowStrength;
};

float SampleShadowFactor(float3 worldPos, float3 normal, float3 lightDirection)
{
    float shadowFactor = 1.0f;
    if (shadowEnabled == 0u)
    {
        return shadowFactor;
    }

    float4 shadowClip = mul(float4(worldPos, 1.0f), lightViewProjection);
    const float invW = 1.0f / max(shadowClip.w, 0.0001f);
    const float3 shadowNdc = shadowClip.xyz * invW;
    const float2 shadowUv = float2(shadowNdc.x * 0.5f + 0.5f, -shadowNdc.y * 0.5f + 0.5f);

    if (shadowUv.x < 0.0f || shadowUv.x > 1.0f || shadowUv.y < 0.0f || shadowUv.y > 1.0f || shadowNdc.z <= 0.0f || shadowNdc.z >= 1.0f)
    {
        return shadowFactor;
    }

    const float depthBias = max(0.00035f * (1.0f - saturate(dot(normal, normalize(-lightDirection)))), 0.0001f);
    const float compareDepth = shadowNdc.z - depthBias;
    float visibility = 0.0f;

    visibility += compareDepth <= shadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(-0.5f, -0.5f)).r ? 1.0f : 0.0f;
    visibility += compareDepth <= shadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(0.5f, -0.5f)).r ? 1.0f : 0.0f;
    visibility += compareDepth <= shadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(-0.5f, 0.5f)).r ? 1.0f : 0.0f;
    visibility += compareDepth <= shadowMapTex.Sample(shadowSampler, shadowUv + shadowMapTexelSize * float2(0.5f, 0.5f)).r ? 1.0f : 0.0f;
    visibility *= 0.25f;

    shadowFactor = lerp(1.0f - shadowStrength, 1.0f, visibility);
    return shadowFactor;
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
    float3 lightContribution = 0.0f.xxx;
    if (light.enabled == 0u || lightType == 0u)
    {
        return lightContribution;
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
            return lightContribution;
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

    const float NdotL = max(dot(normal, L), 0.0f);
    const float shadowFactor = lightType == 3u ? SampleShadowFactor(worldPos, normal, light.direction) : 1.0f;
    const float3 diffuse = light.intensity * light.color * NdotL / max(attenuation, 0.0001f);

    const float3 reflectionDir = reflect(-L, normal);
    const float specularFactor = pow(max(dot(viewDir, reflectionDir), 0.0f), specularPower);
    const float3 specular = specularIntensity * specularColor * specularFactor * light.intensity / max(attenuation, 0.0001f);

    lightContribution = (diffuse + specular) * shadowFactor;
    return lightContribution;
}

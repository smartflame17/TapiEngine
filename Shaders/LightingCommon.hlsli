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

    const float NdotL = max(dot(normal, L), 0.0f);
    const float3 diffuse = light.intensity * light.color * NdotL / max(attenuation, 0.0001f);

    const float3 reflectionDir = reflect(-L, normal);
    const float specularFactor = pow(max(dot(viewDir, reflectionDir), 0.0f), specularPower);
    const float3 specular = specularIntensity * specularColor * specularFactor * light.intensity / max(attenuation, 0.0001f);

    return diffuse + specular;
}

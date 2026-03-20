cbuffer MaterialCbuf : register(b0)
{
    float3 materialColor;
    float specularIntensity;
    float specularPower;
    float3 specularColor;
};

cbuffer LightCbuf : register(b1)
{
    float3 lightpos;
    float diffuseIntensity;
    float3 ambientColor;
    float attConst;
    float3 diffuseColor;
    float attLinear;
    float attQuad;
    float3 lightPadding;
};

cbuffer CameraCbuf : register(b2)
{
    float3 cameraPos;
    float cameraPadding;
};

Texture2D baseColorTex : register(t0);
SamplerState splr : register(s0);

float4 main(float3 worldPos : POSITION, float3 n : NORMAL, float2 texCoord : TEXCOORD) : SV_TARGET
{
    const float3 N = normalize(n);

    const float3 vToLight = lightpos - worldPos;
    const float distance = length(vToLight);
    const float3 L = normalize(vToLight);

    const float att = attConst + attLinear * distance + attQuad * distance * distance;

    const float3 sampledColor = baseColorTex.Sample(splr, texCoord).rgb * materialColor;
    const float3 diffuse = diffuseIntensity * diffuseColor * max(dot(N, L), 0.0f) / att;

    const float3 viewDir = normalize(cameraPos - worldPos);
    const float3 reflectionDir = reflect(-L, N);
    const float specularFactor = pow(max(dot(viewDir, reflectionDir), 0.0f), specularPower);
    const float3 specular = specularIntensity * specularColor * specularFactor / att;

    const float3 color = saturate(sampledColor * (diffuse + ambientColor) + specular);

    return float4(color, 1.0f);
}

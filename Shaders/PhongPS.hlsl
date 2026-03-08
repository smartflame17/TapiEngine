cbuffer MaterialCbuf : register(b0)
{
    float3 materialColor;
    float padding;
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

float4 main(float3 worldPos: POSITION, float3 n : NORMAL) : SV_TARGET
{
    // fragment to light vector
    const float3 vToLight = lightpos - worldPos;
    const float distance = length(vToLight);
    const float3 L = normalize(vToLight);

    // attenuation
    const float att = attConst + attLinear * distance + attQuad * distance * distance;

    // diffuse + ambient lighting modulated by material color
    const float3 diffuse = diffuseIntensity * diffuseColor * max(dot(n, L), 0.0f) / att;
    const float3 color = materialColor * saturate(diffuse + ambientColor);

    return float4(color, 1.0f);
}

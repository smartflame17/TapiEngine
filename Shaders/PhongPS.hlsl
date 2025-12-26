cbuffer LightCbuf
{
    float3 lightpos;
};

static const float3 materialColor = float3(0.7f, 0.7f, 1.0f);
static const float3 ambientColor = float3(0.1f, 0.1f, 0.1f);
static const float3 diffuseColor = float3(1.0f, 1.0f, 1.0f);
static const float diffuseIntensity = 1.0f;

// light attenuation parameters
static const float attConst = 1.0f;
static const float attLinear = 0.045f;
static const float attQuad = 0.0075f;

float4 main(float3 worldPos: POSITION, float3 n : NORMAL) : SV_TARGET
{
    //fragment to light vector
    const float3 vToLight = lightpos - worldPos;
    const float distance = length(vToLight);
    const float3 L = normalize(vToLight);
    
    //attenuation
    const float att = attConst + attLinear * distance + attQuad * distance * distance;
    
    //diffuse
    const float3 diffuse = diffuseIntensity * diffuseColor * max(dot(n, L), 0.0f) / att;
    
    return float4(saturate(diffuse + ambientColor), 1.0f);
}
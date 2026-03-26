cbuffer CBuf
{
    matrix modelViewProj;
    matrix model;
};

struct VSOut
{
    float3 worldPos : Position;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 tex : TexCoord;
    float4 pos : SV_POSITION;
};

VSOut main(float3 pos : POSITION, float3 n : NORMAL, float2 tex : TEXCOORD, float3 tangent : TANGENT)
{
    VSOut vso;
    vso.worldPos = (float3)mul(float4(pos, 1.0f), model);
    vso.normal = normalize(mul(n, (float3x3)model).xyz);
    vso.tangent = normalize(mul(tangent, (float3x3)model).xyz);
    vso.tex = tex;
    vso.pos = mul(float4(pos, 1.0f), modelViewProj);
    return vso;
}

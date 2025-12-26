cbuffer CBuf
{
    matrix modelViewProj;
    matrix model;
};

struct VSOut
{
    float3 worldPos : Position;
    float3 normal : NORMAL;
    float4 pos : SV_POSITION;
};

VSOut main( float3 pos : POSITION, float3 n : NORMAL )
{
    VSOut vso;
    vso.worldPos = (float3) mul(float4(pos, 1.0f), model);
    vso.normal = normalize(mul(n, (float3x3) model).xyz);   // no translating normals
    vso.pos = mul(float4(pos, 1.0f), modelViewProj);
	return vso;
}
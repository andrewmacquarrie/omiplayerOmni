cbuffer perApplication : register(b0)
{
    float4x4 proj;
};

cbuffer perFrame : register(b1)
{
    float4x4 modelview;
    float4x4 texmat;
};

struct VertexIn
{
    float3 pos : POSITION;
    float2 tco : TEXCOORD;
};

struct VertexOut
{
    float2 tco[3] : TEXCOORD;
    float4 pos : SV_POSITION;
};

VertexOut main(VertexIn in_v)
{
    VertexOut out_v;

    float4x4 mvp = mul(proj, modelview);
    out_v.pos = mul(mvp, float4(in_v.pos, 1.0f));

	out_v.tco[0] = mul(texmat, float4(in_v.tco, 0, 1));
    return out_v;
}

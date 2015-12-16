Texture2D Texture : register(t0);
sampler   Sampler : register(s0);

struct Input
{
    float2 tco[3] : TEXCOORD;
};

float4 main(Input in_data) : SV_TARGET
{
    float y = Texture.Sample(Sampler, in_data.tco[0]).r;
    float u = Texture.Sample(Sampler, in_data.tco[1]).r;
    float v = Texture.Sample(Sampler, in_data.tco[2]).r;
    float4 channels = float4(y, u, v, 1.0);
    float4x4 conversion = float4x4( 
        1.000,  0.000,  1.402, -0.701,
        1.000, -0.344, -0.714,  0.529,
        1.000,  1.772,  0.000, -0.886,
        0.000,  0.000,  0.000,  0.000);

    return mul(conversion, channels);
}

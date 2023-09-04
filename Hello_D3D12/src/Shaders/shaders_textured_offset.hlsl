
 struct Node
{
    float4 offset;
};

cbuffer SceneConstantBuffer : register (b0)
{
    int nodeIdx;
    Node nodes[2];
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    float4 color: COLOR;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position: POSITION, float4 color: COLOR, float4 uv: TEXCOORD)
{
    PSInput result;

    result.position = position + nodes[nodeIdx].offset;
    result.color = color;
    result.uv = uv;

    return result;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 col = g_texture.Sample(g_sampler, input.uv);
    col *= input.color;
    
    return col;
};
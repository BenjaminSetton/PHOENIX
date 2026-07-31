struct VSInput
{
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float2 uv  : TEXCOORD0;
};

struct VSOutput
{
    float3 worldPos : POSITION;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.worldPos = input.pos;
    output.uv = input.uv;
    return output;
}

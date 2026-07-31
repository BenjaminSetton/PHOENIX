struct VSInput
{
    [[vk::location(0)]] float2 inPos : POSITION;
    [[vk::location(1)]] float2 inUV  : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 outUV : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.inPos, 0.0, 1.0);
    output.outUV = input.inUV;
    return output;
}

struct VSInput
{
    [[vk::location(0)]] float4 pos   : POSITION;
    [[vk::location(1)]] float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float4 color : COLOR;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = input.pos;
    output.color = input.color;
    return output;
}

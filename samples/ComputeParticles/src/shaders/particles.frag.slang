struct PSInput
{
    [[vk::location(0)]] float4 color : COLOR;
};

[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    return input.color;
}

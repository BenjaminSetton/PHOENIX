struct PSInput
{
    [[vk::location(0)]] float4 color : COLOR;
};

struct Test
{
    float time;
};

[[vk::binding(0, 0)]]
ConstantBuffer<Test> testUBO;

[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    float4 col = input.color;
    col += sin(testUBO.time) * 0.5 + cos(testUBO.time) + 0.1;
    return col;
}

struct VSInput
{
    [[vk::location(0)]] float2 pos   : POSITION;
    [[vk::location(1)]] float2 uv    : TEXCOORD0;
    [[vk::location(2)]] float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv    : TEXCOORD0;
    [[vk::location(1)]] float4 color : COLOR;
};

struct Uniforms
{
    float2 scale;
    float2 translation;
};

[[vk::binding(0, 0)]]
ConstantBuffer<Uniforms> uniforms;

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.pos * uniforms.scale + uniforms.translation, 0.0, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

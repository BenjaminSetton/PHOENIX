struct Camera
{
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(0, 0)]]
ConstantBuffer<Camera> cam;

struct VSInput
{
    [[vk::location(0)]] float3 pos : POSITION;
};

struct VSOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(cam.proj, mul(cam.view, float4(input.pos, 1.0)));
    return output;
}

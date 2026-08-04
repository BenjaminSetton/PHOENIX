// Debug line vertex shader for frustum visualization
// Draws line lists. Each vertex has a position and a color.
//
// Binding layout (set 0):
//   0: CameraBuffer (UBO, read) — view + proj matrices

struct VSInput
{
    [[vk::location(0)]] float3 inPosition : POSITION;
    [[vk::location(1)]] float3 inColor    : COLOR;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 fragColor : COLOR;
};

struct CameraData
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraData> camera;

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPos = float4(input.inPosition, 1.0);
    output.position = mul(camera.projMatrix, mul(camera.viewMatrix, worldPos));
    output.fragColor = input.inColor;
    return output;
}

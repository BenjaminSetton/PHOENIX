// Debug line fragment shader for frustum visualization

struct PSInput
{
    [[vk::location(0)]] float3 fragColor : COLOR;
};

[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    return float4(input.fragColor, 1.0);
}

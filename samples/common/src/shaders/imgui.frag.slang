#include "common.slang"

struct PSInput
{
    [[vk::location(0)]] float2 uv    : TEXCOORD0;
    [[vk::location(1)]] float4 color : COLOR;
};

[[vk::binding(0, 1)]]
Sampler2D fontAtlas;

[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    float3 linearColor = SrgbToLinear(input.color.rgb);
    return float4(linearColor, input.color.a) * fontAtlas.Sample(input.uv);
}

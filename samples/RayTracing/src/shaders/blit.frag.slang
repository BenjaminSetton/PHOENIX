#include "common.slang"

struct PSInput
{
    [[vk::location(0)]] float2 inUV : TEXCOORD0;
};

[[vk::binding(0, 0)]] Sampler2D rayTracingOutput;

static const float exposure = 1.0;

[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    float3 hdrColor = rayTracingOutput.Sample(input.inUV).rgb;
    float3 exposed = hdrColor * exposure;
    float3 toned = ACESFilm(exposed);
    return float4(toned, 1.0);
}

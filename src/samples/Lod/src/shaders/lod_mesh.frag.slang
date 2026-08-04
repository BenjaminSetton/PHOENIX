// LOD mesh fragment shader
// Simple directional lighting with frustum-cull debug coloring:
//   green = inside the LOD camera's frustum, red = frustum-culled.
// LOD level is still conveyed by the mesh shape (sphere / cylinder / cube).

#include "common.slang"

struct PSInput
{
    [[vk::location(0)]] float3 fragNormal : NORMAL;
    [[vk::location(1)]] float2 fragUV     : TEXCOORD0;
    [[vk::location(2)]] uint   fragCulled : CULLED;
};

[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    // Simple directional lighting
    float3 lightDir = normalize(float3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(input.fragNormal), lightDir), 0.2);

    // Red when frustum-culled, green when inside the frustum
    float3 baseColor = (input.fragCulled != 0u) ? float3(0.9, 0.15, 0.15) : float3(0.15, 0.9, 0.2);

    return float4(baseColor * diff, 1.0);
}

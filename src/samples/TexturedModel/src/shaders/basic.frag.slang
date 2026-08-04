// Resources:
// PBR (Victor Gordan) - https://www.youtube.com/watch?v=RRE-F57fbXw&t=10s
// PBR explanation (OGL Dev) - https://www.youtube.com/watch?v=XK_p2MxGBQs&t=126s
// Diffuse IBL (LearnOpenGL) - https://learnopengl.com/PBR/IBL/Diffuse-irradiance

// Simplified rendering equation:
// vec3 finalColor = ( DiffuseBRDF + SpecularBRDF ) * LightIntensity * NdotL

// Explanation of the components of the equation above, assuming the Cook-Torrance BRDF function:
// [ DiffuseBRDF ]
// kD * Lambert / PI

// [ SpecularBRDF ]
// kS * CookTorrance()

// [ LightIntensity ]
// N

// [NdotL]
// dot( surfaceNormal, lightVector )

// Explanation of [ DiffuseBRDF ]:
// - kD and kS represent the ratio between the refraction and reflection components. Their sum must always equal 1, otherwise
//   the law of conservation of energy is violated and this isn't a PBR shader anymore.
// - Lambert represents the material color (retrieved from diffuse texture). This means that the diffuse BRDF value is only applied
//   to dielectrics, and NOT metals!

// Explanation of [ SpecularBRDF ]:
// - The CookTorrance() function is, itself, made up of a separate equation, namely: (D * G * F) / 4 * dot(normal, light) * dot(normal, view).
//   D (normal distribution function), G (geometry function), and F (fresnel function) are all functions as well
// - D represents the number of microfacets that reflect light back at the viewer. More specifically, it tells us how many microfacets have their
//   half-vector aligned with the surface normal. If the half-vector is completely aligned with the normal, this means that ALL the light is being
//   reflected towards the viewer.
// - The function we use for D is GGX (Trowbridge & Reitz), given below:
//      roughness^2 / (PI * ( dot( normal, halfVector )^2 * ( roughness^2 - 1 ) + 1 )^2
//   When this function is 0 it implies an ideal smooth surface, while a value of 1 implies maximum roughness
// - G approximates the self-shadowing factor of microfacets. It tells us the probability that any given microfacet with it's normal is
//   visible to both the light source and the viewer
// - The function we use for G is GGX (Schlick & Beckmann), given below:
//      dot( normal, view ) / ( dot( normal, view ) * ( 1 - K ) + K )
//      where K = ( roughness + 1 )^2 / 8
// - F approximates the fresnel effect, where light bouncing off a lower degree of incidence will be reflected more often than light reflecting with
//   a higher degree of incidence (aka closer to the surface's normal).
// - The function we use for F is the Schlick approximation, given below:
//      baseReflectivity + ( 1 - baseReflectivity ) * ( 1 - dot( view, half ) )^5
//   NOTE: The base reflectivity changes over the visible spectrum, which is why it's represented as an RGB value instead of a
//         float like most other values in the PBR equation. Common practice is to use a value of 0.04 (across all RGB values)
//         for all dielectric materials, but to use the real base reflectivity for metals (which can simply be looked up)

#include "pbr.slang"

struct PSInput
{
    [[vk::location(0)]] float3   inWorldPosition : WORLD_POS;
    [[vk::location(1)]] float3   inNormal        : NORMAL;
    [[vk::location(2)]] float2   inUV            : TEXCOORD0;
    [[vk::location(3)]] float3x3 inTBN           : TBN;
};

[[vk::binding(0, 1)]] Sampler2D diffuseSampler;
[[vk::binding(1, 1)]] Sampler2D normalSampler;
[[vk::binding(2, 1)]] Sampler2D metallicSampler;
[[vk::binding(3, 1)]] Sampler2D roughnessSampler;
[[vk::binding(4, 1)]] Sampler2D lightMapSampler;

struct CameraData
{
    float3 cameraPos;
};

[[vk::binding(0, 2)]] ConstantBuffer<CameraData> cameraData;

// NOTE - The light vector must be pointing TOWARDS the light source
[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    // Calculate the normal from the normal map
    float3 normal = normalSampler.Sample(input.inUV).rgb;
    normal = normal * 2.0 - 1.0;
    // mul(tangentNormal, inTBN) matches the original GLSL `inTBN * normal`
    // (inTBN rows are T, B, N, so this treats them as the TBN columns).
    normal = normalize(mul(normal, input.inTBN));

    float3 cameraPos = cameraData.cameraPos;
    float3 light = -normalize(float3(0.0, -1.0, 0.5));
    float3 view = normalize(cameraPos - input.inWorldPosition);
    float3 albedo = diffuseSampler.Sample(input.inUV).rgb;

    float lightIntensity = 1.0; // NOTE - This is only for point / spotlights, which we do not support right now
    float metalness = metallicSampler.Sample(input.inUV).b;
    float roughness = roughnessSampler.Sample(input.inUV).g;
    float ao = lightMapSampler.Sample(input.inUV).r;

    // F0 (base reflectivity) is 0.04 for dielectrics, and the albedo for metals
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metalness);

    // kS is the Fresnel term; kD is the remaining energy for diffuse
    float NdotL = max(dot(normal, light), 0.0);
    float HdotV = max(dot(normalize(view + light), view), 0.0);
    float3 kS = FresnelSchlick(HdotV, F0);
    float3 kD = float3(1.0, 1.0, 1.0) - kS;

    // Kill diffuse component if we're dealing with a metal
    //kD *= 1.0 - metalness;

    float3 pbrColor = (DiffuseBRDF(kD, albedo) + SpecularBRDF(normal, view, light, F0, roughness)) * lightIntensity * NdotL;

    // Add per-pixel AO
    pbrColor *= ao;

    // Add some ambient lighting
    float3 ambient = float3(0.1, 0.1, 0.1) * albedo;
    pbrColor += ambient;

    return float4(pbrColor, 1.0);
}

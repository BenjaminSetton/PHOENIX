#ifndef PBR_SLANG
#define PBR_SLANG

#include "common.slang"

// GGX/Trowbridge-Reitz normal distribution function
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 0.0001);
}

// Schlick-Smith geometry function (with k remap for direct lighting)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Schlick Fresnel approximation
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Roughness-aware Fresnel, used for the ambient/environment specular term
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 maxF = max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0);
    return F0 + (maxF - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX importance sampling for specular bounces
// Returns a half-vector sampled from the GGX distribution
float3 SampleGGX(float2 rand, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * rand.x;
    float cosTheta = sqrt((1.0 - rand.y) / (1.0 + (a * a - 1.0) * rand.y));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));

    float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Build ONB from N
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

#endif // PBR_SLANG

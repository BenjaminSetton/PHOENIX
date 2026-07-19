#ifndef PBR_GLSL
#define PBR_GLSL

const float PI = 3.14159265359;

// GGX/Trowbridge-Reitz normal distribution function
float DistributionGGX(vec3 N, vec3 H, float roughness)
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

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Schlick Fresnel approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Roughness-aware Fresnel, used for the ambient/environment specular term
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    vec3 maxF = max(vec3(1.0 - roughness), F0);
    return F0 + (maxF - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Cosine-weighted hemisphere sampling for diffuse bounces
// Returns a direction in hemisphere around N, sampled with cosine distribution
vec3 sampleCosineWeightedHemisphere(vec2 rand, vec3 N)
{
    float r = sqrt(rand.x);
    float phi = 2.0 * PI * rand.y;
    vec3 localDir = vec3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - rand.x)));

    // Build ONB from N
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    return normalize(tangent * localDir.x + bitangent * localDir.y + N * localDir.z);
}

// GGX importance sampling for specular bounces
// Returns a half-vector sampled from the GGX distribution
vec3 sampleGGX(vec2 rand, vec3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * rand.x;
    float cosTheta = sqrt((1.0 - rand.y) / (1.0 + (a * a - 1.0) * rand.y));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));

    vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Build ONB from N
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

#endif // PBR_GLSL

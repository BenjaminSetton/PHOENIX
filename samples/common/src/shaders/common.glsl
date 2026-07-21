#ifndef COMMON_GLSL
#define COMMON_GLSL

const float PI = 3.14159265359;

// PCG hash-based pseudo-random number generator
// Returns a single uint32 hash from a uint32 input
uint pcgHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Returns a float in [0, 1) from a uint32 seed
float randFloat(uint seed)
{
    return float(pcgHash(seed) >> 8) / float(1u << 24);
}

// Returns a vec2 of floats in [0, 1) from a uint32 seed
vec2 randVec2(uint seed)
{
    return vec2(randFloat(seed), randFloat(pcgHash(seed)));
}

// Returns a vec3 of floats in [0, 1) from a uint32 seed
vec3 randVec3(uint seed)
{
    return vec3(randFloat(seed), randFloat(pcgHash(seed)), randFloat(pcgHash(pcgHash(seed))));
}

// ACES filmic tonemapping
vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
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

#endif // COMMON_GLSL

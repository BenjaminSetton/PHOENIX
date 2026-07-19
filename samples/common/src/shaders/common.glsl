#ifndef COMMON_GLSL
#define COMMON_GLSL

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

#endif // COMMON_GLSL

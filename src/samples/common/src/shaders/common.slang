#ifndef COMMON_SLANG
#define COMMON_SLANG

static const float PI = 3.14159265359;
static const float EPSILON = 0.000001;

// Converts a color from sRGB to linear space
float3 SrgbToLinear(float3 srgb)
{
    bool3 cutoff = srgb > float3(0.04045, 0.04045, 0.04045);
    float3 lo = srgb / 12.92;
    float3 hi = pow((srgb + 0.055) / 1.055, float3(2.4, 2.4, 2.4));
    return select(lo, hi, cutoff);
}

// PCG hash-based pseudo-random number generator
// Returns a single uint32 hash from a uint32 input
uint PcgHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Returns a float in [0, 1) from a uint32 seed
float RandFloat(uint seed)
{
    return float(PcgHash(seed) >> 8) / float(1u << 24);
}

// Returns a float2 of floats in [0, 1) from a uint32 seed
float2 RandFloat2(uint seed)
{
    return float2(RandFloat(seed), RandFloat(PcgHash(seed)));
}

// Returns a float3 of floats in [0, 1) from a uint32 seed
float3 RandFloat3(uint seed)
{
    return float3(RandFloat(seed), RandFloat(PcgHash(seed)), RandFloat(PcgHash(PcgHash(seed))));
}

// ACES filmic tonemapping
float3 ACESFilm(float3 x)
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
float3 SampleCosineWeightedHemisphere(float2 rand, float3 N)
{
    float r = sqrt(rand.x);
    float phi = 2.0 * PI * rand.y;
    float3 localDir = float3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - rand.x)));

    // Build ONB from N
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    return normalize(tangent * localDir.x + bitangent * localDir.y + N * localDir.z);
}

// Extract 6 frustum planes from a view-projection matrix.
// Plane normals point inward. Returns array of float4 (normal.xyz, distance).
//
// The matrix is used with the column-vector convention (clip = viewProj * pos, i.e.
// mul(viewProj, pos) in the shaders), so the Gribb-Hartmann planes are built from the
// ROWS of the matrix. In HLSL/Slang, viewProj[i] indexes the i-th row directly.
// Depth range is assumed to be [-1, 1] (GLM default), so the near plane uses row3 + row2.
void ExtractFrustumPlanes(float4x4 viewProj, out float4 planes[6])
{
    planes[0] = viewProj[3] + viewProj[0]; // left
    planes[1] = viewProj[3] - viewProj[0]; // right
    planes[2] = viewProj[3] + viewProj[1]; // bottom
    planes[3] = viewProj[3] - viewProj[1]; // top
    planes[4] = viewProj[3] + viewProj[2]; // near
    planes[5] = viewProj[3] - viewProj[2]; // far

    // Normalize
    [unroll]
    for (int i = 0; i < 6; i++)
    {
        float len = length(planes[i].xyz);
        if (len > 0.0)
        {
            planes[i] /= len;
        }
    }
}

// Test if a sphere (center, radius) is inside or intersects the frustum.
bool IsSphereInFrustum(float4 planes[6], float3 center, float radius)
{
    [unroll]
    for (int i = 0; i < 6; i++)
    {
        float dist = dot(planes[i].xyz, center) + planes[i].w;
        if (dist < -radius)
        {
            return false;
        }
    }
    return true;
}

#endif // COMMON_SLANG

// Particle transform is stored as 4 packed float4 "columns" rather than a real float4x4. The
// compute/vertex shaders never do actual matrix math with it -- it's just a convenient container
// (matches C++ ParticleData::transform, a glm::mat4 which is bit-identical to float4[4] in memory).
// Using an explicit array avoids any row-major/column-major ambiguity for this data.
struct Particle
{
    float4 columns[4]; // col0.xyz = velocity, col1 = (lifetime, scale, maxLifetime, 0), col3.xyz = position
    float4 color;      // rgba
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<Particle> particleBuffer;

struct SimParams
{
    float deltaTime;     // seconds elapsed since last update
    float totalTime;     // total elapsed time in seconds
    float shouldExplode; // Float -> bool
    uint  particleCount;
};

[[vk::binding(1, 0)]]
ConstantBuffer<SimParams> sim;

// Volume bounds (must match m_volumeMinBound / m_volumeMaxBound in C++)
static const float m_volumeMin = -30.0;
static const float m_volumeMax =  30.0;

// Generate a pseudo-random float in [0, 1) from an unsigned seed.
float Hash(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return float(seed & 0x00FFFFFFu) / float(0x01000000u);
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    if (index >= sim.particleCount)
    {
        return;
    }

    Particle particle = particleBuffer[index];

    // Per-particle deterministic randomness.
    float seedA = Hash(index * 2u + 1u);
    float seedB = Hash(index * 7u + 3u);
    float seedC = Hash(index * 13u + 5u);
    float seedD = Hash(index * 41u + 11u);

    // --- Fire simulation ---
    // Transform layout:
    //   column 0.xyz = velocity,     column 0.w = 0
    //   column 1.x   = lifetime,     column 1.y = scale
    //   column 1.z   = max lifetime, column 1.w = 0
    //   column 2.xyz = unused,       column 2.w = 0
    //   column 3.xyz = position,     column 3.w = 1

    float3 position = particle.columns[3].xyz;
    float3 velocity = particle.columns[0].xyz;
    float lifetime    = particle.columns[1].x;
    float scale       = particle.columns[1].y;
    float maxLifetime = particle.columns[1].z;

    float3 explosionCenter = float3(0.0, 0.0, 0.0);
    float explosionDist = 9999.0;
    float explosionForce = 0.0;

    if (sim.shouldExplode > 0.0)
    {
        explosionCenter = float3(
            (Hash(sim.shouldExplode * 7u + 1u) - 0.5) * 40.0,
            float(m_volumeMin) + Hash(sim.shouldExplode * 11u + 3u) * 20.0,
            (Hash(sim.shouldExplode * 13u + 7u) - 0.5) * 40.0
        );
        float3 toParticle = position - explosionCenter;
        explosionDist = length(toParticle);
        float explosionRadius = 12.0;
        if (explosionDist < explosionRadius)
        {
            float falloff = 1.0 - explosionDist / explosionRadius;
            explosionForce = falloff * falloff * 40.0;
            velocity += normalize(toParticle + float3(0.001, 0.001, 0.001)) * explosionForce;
        }
    }

    // Buoyancy: hot air rises. World-space +Y is up on screen.
    velocity.y += 5.0 * sim.deltaTime;

    // Drag to prevent particles from accelerating forever.
    velocity *= (1.0 - 0.5 * sim.deltaTime);

    // Integrate.
    position += velocity * sim.deltaTime;
    lifetime -= sim.deltaTime;

    // Kill particles when they leave the volume
    if (position.x < float(m_volumeMin) || position.x > float(m_volumeMax) ||
        position.y < float(m_volumeMin) || position.y > float(m_volumeMax) ||
        position.z < float(m_volumeMin) || position.z > float(m_volumeMax))
    {
        lifetime = 0.0;
    }

    // Respawn when lifetime expires or particle rises above the volume.
    if (lifetime <= 0.0)
    {
        // Spawn at the bottom plane with random horizontal spread.
        float r1 = Hash(index * 101u + uint(sim.totalTime * 7.0));
        float r2 = Hash(index * 257u + uint(sim.totalTime * 11.0));
        float r3 = Hash(index * 401u + uint(sim.totalTime * 5.0));
        position = float3(
            (r1 - 0.5) * 24.0,
            float(m_volumeMin) + 1.0,
            (r2 - 0.5) * 24.0
        );

        // Random upward velocity.
        velocity = float3(
            (seedA - 0.5) * 10.0,
            1.0 + seedB * 6.0 + r3 * 3.0,
            (seedC - 0.5) * 10.0
        );

        maxLifetime = 1.5 + seedD * 3.0;
        lifetime = maxLifetime;
    }

    // Calculate properties based on remaining lifetime
    float lifetimePct = lifetime / maxLifetime; // 0 - 1
    float invExpLifetimePct = (1.0 - lifetimePct) * (1.0 - lifetimePct);
    float turbulence = 5.0 * invExpLifetimePct;
    float minScale = 0.2;

    scale = lifetimePct + minScale; // Prevent from going all the way to 0
    velocity.x += turbulence * sim.deltaTime;
    velocity.y += 5.0 * (invExpLifetimePct + 0.8) * sim.deltaTime;
    velocity.z += turbulence * sim.deltaTime;

    // --- Rebuild transform ---
    particle.columns[0] = float4(velocity,     0.0);
    particle.columns[1] = float4(lifetime, scale, maxLifetime, 0.0);
    particle.columns[2] = float4(0.0, 0.0, 1.0, 0.0);
    particle.columns[3] = float4(position,    1.0);

    // Color - temperature gradient based on lifetime
    float heat = lifetimePct;

    // Temperature curve: sharp white->orange near base, gradual red->dark higher up.
    float3 color;
    if (heat > 0.8)
    {
        // White-hot core.
        color = lerp(float3(1.0, 0.6, 0.2), float3(1.0, 1.0, 0.9), (heat - 0.8) * 5.0);
    }
    else if (heat > 0.3)
    {
        // Orange to red.
        float t2 = (heat - 0.3) / 0.5;
        color = lerp(float3(0.8, 0.15, 0.02), float3(1.0, 0.6, 0.2), t2);
    }
    else
    {
        // Red to dark smoke.
        color = lerp(float3(0.1, 0.05, 0.05), float3(0.8, 0.15, 0.02), heat / 0.3);
    }

    // Explosion flash: particles caught in the blast glow white-hot briefly.
    float explosionFlash = 0.0;
    if (explosionForce > 0.0)
    {
        explosionFlash = clamp(explosionForce / 40.0, 0.0, 1.0);
    }
    color = lerp(color, float3(1.0, 1.0, 0.95), explosionFlash);

    particle.color = float4(color * (0.5 + heat * 0.5), 1.0);

    particleBuffer[index] = particle;
}

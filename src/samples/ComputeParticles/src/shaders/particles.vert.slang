// Particle transform is stored as 4 packed float4 "columns" rather than a real float4x4 -- see
// particles.comp.slang for details.
struct Particle
{
    float4 columns[4];
    float4 color;
};

[[vk::binding(0, 0)]]
StructuredBuffer<Particle> particleBuffer;

struct Camera
{
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(1, 0)]]
ConstantBuffer<Camera> cam;

// Two triangles (6 vertices) forming a unit quad centered on the origin, in local space.
static const float2 corners[6] = {
    float2(-0.1, -0.1), float2( 0.1, -0.1), float2( 0.1,  0.1),
    float2(-0.1, -0.1), float2( 0.1,  0.1), float2(-0.1,  0.1)
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float4 color : COLOR;
};

[shader("vertex")]
VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;

    uint particleIndex = vertexID / 6u;
    uint cornerIndex   = vertexID % 6u;

    Particle particle = particleBuffer[particleIndex];

    // Position is stored in column 3; scale is encoded in column 1's y component.
    float3 worldPos = particle.columns[3].xyz;
    float scale     = particle.columns[1].y;

    // Camera-facing billboard: extract right and up from the view matrix's rows, which give
    // the camera's world-space right/up axes (view = inverse of the camera's world transform).
    float3 camRight = cam.view[0].xyz;
    float3 camUp    = cam.view[1].xyz;

    float2 corner = corners[cornerIndex] * scale;
    float3 finalPos = worldPos + camRight * corner.x + camUp * corner.y;

    output.position = mul(cam.proj, mul(cam.view, float4(finalPos, 1.0)));
    output.color = particle.color;

    return output;
}

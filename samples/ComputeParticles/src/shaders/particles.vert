#version 450

struct Particle
{
    mat4 transform; // model matrix (position / rotation / scale)
    vec4 color;     // rgba
};

layout(std140, set = 0, binding = 0) readonly buffer ParticleBuffer
{
    Particle particles[];
} particleBuffer;

layout(std140, set = 0, binding = 1) uniform Camera
{
    mat4 view;
    mat4 proj;
} cam;

layout(location = 0) out vec4 vColor;

// Two triangles (6 vertices) forming a unit quad centered on the origin, in local space.
const vec2 corners[6] = vec2[](
    vec2(-0.5, -0.5), vec2( 0.5, -0.5), vec2( 0.5,  0.5),
    vec2(-0.5, -0.5), vec2( 0.5,  0.5), vec2(-0.5,  0.5)
);

void main()
{
    uint particleIndex = uint(gl_VertexIndex) / 6u;
    uint cornerIndex   = uint(gl_VertexIndex) % 6u;

    Particle particle = particleBuffer.particles[particleIndex];

    // Expand the point into a quad corner in the particle's local space, then apply
    // its transform (from the compute pass) and the camera view / projection.
    vec4 localPos = vec4(corners[cornerIndex], 0.0, 1.0);
    gl_Position = cam.proj * cam.view * particle.transform * localPos;

    vColor = particle.color;
}

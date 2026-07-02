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
    vec2(-0.1, -0.1), vec2( 0.1, -0.1), vec2( 0.1,  0.1),
    vec2(-0.1, -0.1), vec2( 0.1,  0.1), vec2(-0.1,  0.1)
);

void main()
{
    uint particleIndex = uint(gl_VertexIndex) / 6u;
    uint cornerIndex   = uint(gl_VertexIndex) % 6u;

    Particle particle = particleBuffer.particles[particleIndex];

    // Position is stored in column 3; scale is encoded in column 1's y component.
    vec3 worldPos = particle.transform[3].xyz;
    float scale   = particle.transform[1].y;

    // Camera-facing billboard: extract right and up from the inverse view matrix.
    // The view matrix rows (transposed) give us the camera's right and up directions.
    vec3 camRight = vec3(cam.view[0][0], cam.view[1][0], cam.view[2][0]);
    vec3 camUp    = vec3(cam.view[0][1], cam.view[1][1], cam.view[2][1]);

    vec2 corner = corners[cornerIndex] * scale;
    vec3 finalPos = worldPos + camRight * corner.x + camUp * corner.y;

    gl_Position = cam.proj * cam.view * vec4(finalPos, 1.0);

    vColor = particle.color;
}

#version 450

layout(location = 0) in vec3 inPos;

layout(std140, set = 0, binding = 0) uniform Camera
{
    mat4 view;
    mat4 proj;
} cam;

void main()
{
    gl_Position = cam.proj * cam.view * vec4(inPos, 1.0);
}

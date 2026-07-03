#version 450

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(set = 0, binding = 0) uniform Uniforms {
    vec2 uScale;
    vec2 uTranslation;
};

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main()
{
    gl_Position = vec4(aPos * uScale + uTranslation, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}

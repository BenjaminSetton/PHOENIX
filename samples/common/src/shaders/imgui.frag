#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(set = 1, binding = 0) uniform sampler2D uFontAtlas;

layout(location = 0) out vec4 outColor;

vec3 srgbToLinear(vec3 srgb)
{
    return mix(srgb / 12.92, pow((srgb + 0.055) / 1.055, vec3(2.4)), greaterThan(srgb, vec3(0.04045)));
}

void main()
{
    vec3 linearColor = srgbToLinear(vColor.rgb);
    outColor = vec4(linearColor, vColor.a) * texture(uFontAtlas, vUV);
}

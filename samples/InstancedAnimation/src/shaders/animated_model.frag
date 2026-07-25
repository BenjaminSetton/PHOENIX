#version 460

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(fragNormal), lightDir), 0.2);

    vec4 texColor = texture(diffuseTexture, fragUV);
    outColor = vec4(texColor.rgb * diff, texColor.a);
}

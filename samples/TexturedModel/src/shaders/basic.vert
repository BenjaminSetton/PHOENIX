#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBitangent;
layout(location = 4) in vec2 inUV;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out mat3 outTBN;

layout(set = 0, binding = 0) uniform Transform
{
    mat4 world;
    mat4 invView;
    mat4 proj;
}  transformUBO;

void main()
{
    gl_Position = transformUBO.proj * transformUBO.invView * transformUBO.world * vec4(inPosition, 1.0);

    // Calculate the output variables going to the pixel shader
    outWorldPosition = (transformUBO.world * vec4(inPosition, 1.0)).xyz;

    outNormal = normalize((transformUBO.world * vec4(inNormal, 0.0)).xyz);
    outUV = inUV;

    // Construct the TBN matrix
    vec3 T = normalize((transformUBO.world * vec4(inTangent, 0.0)).xyz);
    vec3 N = outNormal;

    // Re-orthogonalize the TBN matrix in case the tangents are interpolated between vertices (using Gram-Schmidt process)
    T = normalize(T - dot(T, N) * N);

    //vec3 B = cross(N, T);
    vec3 B = normalize((transformUBO.world * vec4(inBitangent, 0.0)).xyz);

    // TBN must form a right handed coord system.
    // Some models have symmetric UVs. Check and fix.
    if (dot(cross(N, T), B) < 0.0)
    {
        T = T * -1.0;
    }

    outTBN = mat3(T, B, N);
}
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadInEXT vec3 hitColor;

hitAttributeEXT vec2 baryCoords;

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec2 uv;
};

layout(set = 0, binding = 2) buffer Vertices
{
    Vertex v[];
} vertices;

layout(set = 0, binding = 3) buffer Indices
{
    uint i[];
} indices;

struct GeometryInfo
{
    uint firstVertex;
    uint firstIndex;
    uint materialIndex;
    uint textureIndex;
};

layout(set = 0, binding = 4) buffer GeometryInfoBuffer
{
    GeometryInfo g[];
} geometryInfo;

#define MAX_TEXTURES 128
layout(set = 2, binding = 0) uniform sampler2D textures[MAX_TEXTURES];

void main()
{
    uint geometryIndex = gl_InstanceCustomIndexEXT;
    GeometryInfo info = geometryInfo.g[geometryIndex];

    uint primitiveIndex = gl_PrimitiveID;

    uint i0 = indices.i[info.firstIndex + primitiveIndex * 3 + 0] + info.firstVertex;
    uint i1 = indices.i[info.firstIndex + primitiveIndex * 3 + 1] + info.firstVertex;
    uint i2 = indices.i[info.firstIndex + primitiveIndex * 3 + 2] + info.firstVertex;

    Vertex v0 = vertices.v[i0];
    Vertex v1 = vertices.v[i1];
    Vertex v2 = vertices.v[i2];

    vec3 bary = vec3(1.0 - baryCoords.x - baryCoords.y, baryCoords.x, baryCoords.y);
    vec3 normal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    vec2 uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;

    vec3 lightDir = normalize(vec3(gl_WorldToObjectEXT * vec4(0.5, 1.0, 0.3, 0.0)));
    float nDotL = max(dot(normal, lightDir), 0.0);

    vec3 baseColor = texture(textures[info.textureIndex], uv).rgb;
    hitColor = baseColor * (0.2 + 0.8 * nDotL);
}

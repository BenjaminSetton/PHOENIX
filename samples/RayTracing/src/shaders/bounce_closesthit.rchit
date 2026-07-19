#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : enable

#include "pbr.glsl"
#include "common.glsl"

layout(location = 2) rayPayloadInEXT vec3 bounceColor;
layout(location = 1) rayPayloadEXT float shadowFactor;

hitAttributeEXT vec2 baryCoords;

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec2 uv;
};

layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;

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
    uint padding;
};

layout(set = 0, binding = 4) buffer GeometryInfoBuffer
{
    GeometryInfo g[];
} geometryInfo;

struct MaterialInfo
{
    uint albedoTexIndex;
    uint normalTexIndex;
    uint metallicTexIndex;
    uint roughnessTexIndex;
    uint aoTexIndex;
    uint specularTexIndex;
    uint lightmapTexIndex;
    uint padding;
};

layout(set = 0, binding = 5) buffer MaterialBuffer
{
    MaterialInfo m[];
} materialBuffer;

layout(set = 0, binding = 6) uniform samplerCube envCubeMap;

#define MAX_TEXTURES 128
layout(set = 2, binding = 0) uniform sampler2D textures[MAX_TEXTURES];

void main()
{
    uint geometryIndex = gl_InstanceCustomIndexEXT;
    GeometryInfo info = geometryInfo.g[geometryIndex];
    MaterialInfo mat = materialBuffer.m[info.materialIndex];

    uint primitiveIndex = gl_PrimitiveID;

    uint i0 = indices.i[info.firstIndex + primitiveIndex * 3 + 0] + info.firstVertex;
    uint i1 = indices.i[info.firstIndex + primitiveIndex * 3 + 1] + info.firstVertex;
    uint i2 = indices.i[info.firstIndex + primitiveIndex * 3 + 2] + info.firstVertex;

    Vertex v0 = vertices.v[i0];
    Vertex v1 = vertices.v[i1];
    Vertex v2 = vertices.v[i2];

    vec3 bary = vec3(1.0 - baryCoords.x - baryCoords.y, baryCoords.x, baryCoords.y);
    vec2 uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;

    vec3 objNormal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    vec3 objTangent = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;

    mat3 objToWorld = mat3(gl_ObjectToWorldEXT);
    vec3 worldNormal = normalize(objToWorld * objNormal);

    vec3 N = worldNormal;
    if (length(objTangent) > 0.001)
    {
        vec3 worldTangent = normalize(objToWorld * objTangent);
        vec3 worldBitangent = cross(worldNormal, worldTangent);
        mat3 TBN = mat3(worldTangent, worldBitangent, worldNormal);

        vec2 nxy = texture(textures[mat.normalTexIndex], uv).rg * 2.0 - 1.0;
        float nz = sqrt(max(0.0, 1.0 - dot(nxy, nxy)));
        vec3 sampledNormal = vec3(nxy, nz);
        N = normalize(TBN * sampledNormal);
    }

    vec3 worldHitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    vec3 albedo = texture(textures[mat.albedoTexIndex], uv).rgb;
    float metallic = texture(textures[mat.metallicTexIndex], uv).r;
    float roughness = clamp(texture(textures[mat.roughnessTexIndex], uv).r, 0.05, 1.0);
    float ao = texture(textures[mat.aoTexIndex], uv).r;

    vec3 L = normalize(vec3(0.5, 1.0, 0.3));
    vec3 V = normalize(-gl_WorldRayDirectionEXT);
    vec3 H = normalize(V + L);

    shadowFactor = 0.0;
    vec3 shadowOrigin = worldHitPos + worldNormal * 0.003;
    traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xff, 0, 0, 1, shadowOrigin, 0.001, L, 1000.0, 1);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    specular = min(specular, vec3(10.0));

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);

    vec3 radiance = vec3(3.0) * NdotL * shadowFactor;
    vec3 directLight = (kD * albedo / PI + specular) * radiance;

    bounceColor = directLight * ao;
}

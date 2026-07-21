#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(location = 0) rayPayloadInEXT vec3 hitColor;
layout(location = 1) rayPayloadEXT float shadowFactor;
layout(location = 2) rayPayloadEXT vec3 bounceColor;

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
    uint specularTexIndex;
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

    // Interpolate geometric normal, tangent, bitangent in object space
    vec3 objNormal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    vec3 objTangent = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;
    vec3 objBitangent = v0.bitangent * bary.x + v1.bitangent * bary.y + v2.bitangent * bary.z;

    // Transform to world space using gl_ObjectToWorldEXT (3x3 part for directions)
    mat3 objToWorld = mat3(gl_ObjectToWorldEXT);
    vec3 worldNormal = normalize(objToWorld * objNormal);

    // Apply normal mapping with fallback to geometric normal if tangents are missing
    vec3 N = worldNormal;
    if (length(objTangent) > 0.001)
    {
        vec3 worldTangent = normalize(objToWorld * objTangent);
        // Compute bitangent via cross product for consistent right-handed TBN
        // (stored bitangent may be inconsistent after aiProcess_FlipUVs)
        vec3 worldBitangent = cross(worldNormal, worldTangent);
        mat3 TBN = mat3(worldTangent, worldBitangent, worldNormal);

        // Reconstruct tangent-space normal from R/G channels. Normal maps are BC5-compressed
        // (two-channel), so the sampled blue channel is 0 and cannot be used directly.
        // Z is derived assuming a unit-length normal in the +Z hemisphere.
        vec2 nxy = texture(textures[mat.normalTexIndex], uv).rg * 2.0 - 1.0;
        float nz = sqrt(max(0.0, 1.0 - dot(nxy, nxy)));
        vec3 sampledNormal = vec3(nxy, nz);
        N = normalize(TBN * sampledNormal);
    }

    // World-space hit position — use GPU-provided ray hit for precision
    vec3 worldHitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    // Sample material textures
    vec3 albedo = texture(textures[mat.albedoTexIndex], uv).rgb;
    vec3 specularColor = texture(textures[mat.specularTexIndex], uv).rgb;

    // Light direction (fixed directional light)
    vec3 L = normalize(vec3(0.5, 1.0, 0.3));
    vec3 V = normalize(-gl_WorldRayDirectionEXT);
    vec3 H = normalize(V + L);

    // Shadow ray — offset origin along geometric normal to prevent self-intersection
    shadowFactor = 0.0;
    vec3 shadowOrigin = worldHitPos + worldNormal * 0.003;
    traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xff, 0, 0, 1, shadowOrigin, 0.001, L, 1000.0, 1);

    // Blinn-Phong shading
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float shininess = 32.0;

    vec3 diffuse = albedo * NdotL * shadowFactor;
    vec3 specular = specularColor * pow(NdotH, shininess) * NdotL * shadowFactor;

    vec3 directLight = (diffuse + specular) * vec3(3.0);

    // Indirect lighting via one diffuse bounce ray
    uint seed = uint(gl_LaunchIDEXT.x) + uint(gl_LaunchIDEXT.y) * uint(gl_LaunchSizeEXT.x);
    seed = pcgHash(seed + floatBitsToUint(worldHitPos.x) + floatBitsToUint(worldHitPos.y) + floatBitsToUint(worldHitPos.z));

    vec2 rand2 = randVec2(pcgHash(seed));
    vec3 bounceDir = sampleCosineWeightedHemisphere(rand2, N);
    vec3 bounceThroughput = albedo;

    // Trace indirect bounce ray
    vec3 bounceOrigin = worldHitPos + N * 0.003;
    bounceColor = vec3(0.0, 0.0, 0.0);
    traceRayEXT(topLevelAS, 0, 0xff, 1, 0, 0, bounceOrigin, 0.001, bounceDir, 1000.0, 2);

    vec3 indirectLight = bounceThroughput * bounceColor;

    hitColor = directLight + indirectLight;
}

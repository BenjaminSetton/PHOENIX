#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadInEXT vec3 hitColor;
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

#define MAX_TEXTURES 128
layout(set = 2, binding = 0) uniform sampler2D textures[MAX_TEXTURES];

const float PI = 3.14159265359;

// GGX/Trowbridge-Reitz normal distribution function
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 0.0001);
}

// Schlick-Smith geometry function (with k remap for direct lighting)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Schlick Fresnel approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Roughness-aware Fresnel, used for the ambient/environment specular term
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    vec3 maxF = max(vec3(1.0 - roughness), F0);
    return F0 + (maxF - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

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

    // Sample PBR material textures
    vec3 albedo = texture(textures[mat.albedoTexIndex], uv).rgb;
    float metallic = texture(textures[mat.metallicTexIndex], uv).r;
    float roughness = clamp(texture(textures[mat.roughnessTexIndex], uv).r, 0.05, 1.0);
    float ao = texture(textures[mat.aoTexIndex], uv).r;

    // Light direction (fixed directional light)
    vec3 L = normalize(vec3(0.5, 1.0, 0.3));
    vec3 V = normalize(-gl_WorldRayDirectionEXT);
    vec3 H = normalize(V + L);

    // Shadow ray — offset origin along geometric normal to prevent self-intersection
    shadowFactor = 0.0;
    vec3 shadowOrigin = worldHitPos + worldNormal * 0.003;
    traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xff, 0, 0, 1, shadowOrigin, 0.001, L, 1000.0, 1);

    // PBR Cook-Torrance BRDF
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);

    // Direct lighting
    vec3 radiance = vec3(3.0) * NdotL * shadowFactor;
    vec3 directLight = (kD * albedo / PI + specular) * radiance;

    // Ambient (constant-environment IBL approximation).
    // Feeds both diffuse (dielectrics) and Fresnel-weighted specular (metals) so
    // metallic surfaces reflect the ambient environment instead of rendering black.
    const vec3 ambientColor = vec3(0.2);
    float NdotV = max(dot(N, V), 0.0);
    vec3 ambientF = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 ambientKD = (vec3(1.0) - ambientF) * (1.0 - metallic);
    vec3 ambientDiffuse = ambientColor * albedo * ambientKD;
    vec3 ambientSpecular = ambientColor * ambientF;
    vec3 ambient = (ambientDiffuse + ambientSpecular) * ao;

    hitColor = ambient + directLight;
}

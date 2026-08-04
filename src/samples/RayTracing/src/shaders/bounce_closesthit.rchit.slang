#include "common.slang"
#include "pbr.slang"

struct RayPayload
{
    float3 hitColor;
    float shadowFactor;
    float3 bounceColor;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float2 uv;
};

struct GeometryInfo
{
    uint firstVertex;
    uint firstIndex;
    uint materialIndex;
    uint padding;
};

struct MaterialInfo
{
    uint albedoTexIndex;
    uint normalTexIndex;
    uint specularTexIndex;
    uint padding;
};

[[vk::binding(1, 0)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(2, 0)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(3, 0)]] StructuredBuffer<uint> indices;
[[vk::binding(4, 0)]] StructuredBuffer<GeometryInfo> geometryInfo;
[[vk::binding(5, 0)]] StructuredBuffer<MaterialInfo> materialBuffer;
[[vk::binding(6, 0)]] Sampler2DArray envCubeMap;

static const uint MAX_TEXTURES = 128;
[[vk::binding(0, 2)]] Sampler2D textures[MAX_TEXTURES];

[shader("closesthit")]
void main(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attrs)
{
    uint geometryIndex = InstanceIndex();
    GeometryInfo info = geometryInfo[geometryIndex];
    MaterialInfo mat = materialBuffer[info.materialIndex];

    uint primitiveIndex = PrimitiveIndex();

    uint i0 = indices[info.firstIndex + primitiveIndex * 3 + 0] + info.firstVertex;
    uint i1 = indices[info.firstIndex + primitiveIndex * 3 + 1] + info.firstVertex;
    uint i2 = indices[info.firstIndex + primitiveIndex * 3 + 2] + info.firstVertex;

    Vertex v0 = vertices[i0];
    Vertex v1 = vertices[i1];
    Vertex v2 = vertices[i2];

    float3 bary = float3(1.0 - attrs.barycentrics.x - attrs.barycentrics.y, attrs.barycentrics.x, attrs.barycentrics.y);
    float2 uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;

    float3 objNormal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    float3 objTangent = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;

    float3x3 objToWorld = float3x3(
        ObjectToWorld3x4()[0].xyz,
        ObjectToWorld3x4()[1].xyz,
        ObjectToWorld3x4()[2].xyz
    );
    float3 worldNormal = normalize(mul(objToWorld, objNormal));

    float3 N = worldNormal;
    if (length(objTangent) > 0.001)
    {
        float3 worldTangent = normalize(mul(objToWorld, objTangent));
        float3 worldBitangent = cross(worldNormal, worldTangent);
        float3x3 TBN = float3x3(worldTangent, worldBitangent, worldNormal);

        float2 nxy = textures[mat.normalTexIndex].Sample(uv).rg * 2.0 - 1.0;
        float nz = sqrt(max(0.0, 1.0 - dot(nxy, nxy)));
        float3 sampledNormal = float3(nxy, nz);
        N = normalize(mul(sampledNormal, TBN));
    }

    float3 worldHitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    float3 albedo = textures[mat.albedoTexIndex].Sample(uv).rgb;

    // Specular texture: R unused, G roughness, B metalness
    float2 specularSample = textures[mat.specularTexIndex].Sample(uv).gb;
    float roughness = max(specularSample.x, 0.045);
    float metalness = specularSample.y;

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metalness);
    float3 diffuseAlbedo = albedo * (1.0 - metalness);

    float3 L = normalize(float3(0.5, 1.0, 0.3));
    float3 V = normalize(-WorldRayDirection());
    float3 H = normalize(V + L);

    payload.shadowFactor = 0.0;
    float3 shadowOrigin = worldHitPos + worldNormal * 0.003;
    RayDesc shadowRay;
    shadowRay.Origin = shadowOrigin;
    shadowRay.TMin = 0.001;
    shadowRay.Direction = L;
    shadowRay.TMax = 1000.0;
    TraceRay(topLevelAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xff, 0, 0, 1, shadowRay, payload);

    // Cook-Torrance direct lighting, driven by roughness/metalness decoded above
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0001);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 specularBRDF = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);

    float3 diffuse = diffuseAlbedo / PI;
    float3 specular = specularBRDF;

    payload.bounceColor = (diffuse + specular) * NdotL * payload.shadowFactor * float3(3.0);
}

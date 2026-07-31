#include "common.slang"

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

    // Interpolate geometric normal, tangent, bitangent in object space
    float3 objNormal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    float3 objTangent = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;

    // Transform to world space using ObjectToWorld3x4 (GPU-provided, already row-major)
    float3x3 objToWorld = float3x3(
        ObjectToWorld3x4()[0].xyz,
        ObjectToWorld3x4()[1].xyz,
        ObjectToWorld3x4()[2].xyz
    );
    float3 worldNormal = normalize(mul(objToWorld, objNormal));

    // Apply normal mapping with fallback to geometric normal if tangents are missing
    float3 N = worldNormal;
    if (length(objTangent) > 0.001)
    {
        float3 worldTangent = normalize(mul(objToWorld, objTangent));
        // Compute bitangent via cross product for consistent right-handed TBN
        float3 worldBitangent = cross(worldNormal, worldTangent);
        // TBN columns = (tangent, bitangent, normal) — use mul(v, M) to match GLSL mat3(cols) * v
        float3x3 TBN = float3x3(worldTangent, worldBitangent, worldNormal);

        // Reconstruct tangent-space normal from R/G channels. Normal maps are BC5-compressed
        // (two-channel), so the sampled blue channel is 0 and cannot be used directly.
        // Z is derived assuming a unit-length normal in the +Z hemisphere.
        float2 nxy = textures[mat.normalTexIndex].Sample(uv).rg * 2.0 - 1.0;
        float nz = sqrt(max(0.0, 1.0 - dot(nxy, nxy)));
        float3 sampledNormal = float3(nxy, nz);
        N = normalize(mul(sampledNormal, TBN));
    }

    // World-space hit position — use GPU-provided ray hit for precision
    float3 worldHitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // Sample material textures
    float3 albedo = textures[mat.albedoTexIndex].Sample(uv).rgb;
    float3 specularColor = textures[mat.specularTexIndex].Sample(uv).rgb;

    // Light direction (fixed directional light)
    float3 L = normalize(float3(0.5, 1.0, 0.3));
    float3 V = normalize(-WorldRayDirection());
    float3 H = normalize(V + L);

    // Shadow ray — offset origin along geometric normal to prevent self-intersection
    payload.shadowFactor = 0.0;
    float3 shadowOrigin = worldHitPos + worldNormal * 0.003;
    RayDesc shadowRay;
    shadowRay.Origin = shadowOrigin;
    shadowRay.TMin = 0.001;
    shadowRay.Direction = L;
    shadowRay.TMax = 1000.0;
    TraceRay(topLevelAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xff, 0, 0, 1, shadowRay, payload);

    // Blinn-Phong shading
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float shininess = 32.0;

    float3 diffuse = albedo * NdotL * payload.shadowFactor;
    float3 specular = specularColor * pow(NdotH, shininess) * NdotL * payload.shadowFactor;

    float3 directLight = (diffuse + specular) * float3(3.0);

    // Indirect lighting via one diffuse bounce ray
    uint2 launchID = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    uint seed = launchID.x + launchID.y * launchDim.x;
    seed = PcgHash(seed + asuint(worldHitPos.x) + asuint(worldHitPos.y) + asuint(worldHitPos.z));

    float2 rand2 = RandFloat2(PcgHash(seed));
    float3 bounceDir = SampleCosineWeightedHemisphere(rand2, N);
    float3 bounceThroughput = albedo;

    // Trace indirect bounce ray
    float3 bounceOrigin = worldHitPos + N * 0.003;
    payload.bounceColor = float3(0.0, 0.0, 0.0);
    RayDesc bounceRay;
    bounceRay.Origin = bounceOrigin;
    bounceRay.TMin = 0.001;
    bounceRay.Direction = bounceDir;
    bounceRay.TMax = 1000.0;
    TraceRay(topLevelAS, 0, 0xff, 1, 0, 0, bounceRay, payload);

    float3 indirectLight = bounceThroughput * payload.bounceColor;

    payload.hitColor = directLight + indirectLight;
}

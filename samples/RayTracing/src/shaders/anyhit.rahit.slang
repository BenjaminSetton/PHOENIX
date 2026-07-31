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

[[vk::binding(2, 0)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(3, 0)]] StructuredBuffer<uint> indices;
[[vk::binding(4, 0)]] StructuredBuffer<GeometryInfo> geometryInfo;
[[vk::binding(5, 0)]] StructuredBuffer<MaterialInfo> materialBuffer;

static const uint MAX_TEXTURES = 128;
[[vk::binding(0, 2)]] Sampler2D textures[MAX_TEXTURES];

[shader("anyhit")]
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

    // Alpha test: discard fragments with low alpha from the albedo texture
    float alpha = textures[mat.albedoTexIndex].Sample(uv).a;
    if (alpha < 0.5)
    {
        IgnoreHit();
    }
}

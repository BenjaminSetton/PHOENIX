struct FSInput
{
    float3 worldPos   : POSITION;
    float2 uv         : TEXCOORD0;
    float  tessFactor : TEXCOORD1;
};

struct FSOutput
{
    [[vk::location(0)]] float4 color : SV_Target;
};

// Simple Lambertian lighting with a fixed directional light
[shader("fragment")]
FSOutput main(FSInput input)
{
    // Compute normal from the heightmap via finite differences
    float eps = 0.005;
    float hL = sin((input.uv.x - eps) * 8.0) * cos(input.uv.y * 8.0);
    float hR = sin((input.uv.x + eps) * 8.0) * cos(input.uv.y * 8.0);
    float hD = sin(input.uv.x * 8.0) * cos((input.uv.y - eps) * 8.0);
    float hU = sin(input.uv.x * 8.0) * cos((input.uv.y + eps) * 8.0);

    float3 normal = normalize(float3(hL - hR, 2.0 * eps, hD - hU));

    // Fixed directional light
    float3 lightDir = normalize(float3(0.5, 1.0, 0.3));
    float NdotL = max(dot(normal, lightDir), 0.0);

    // Color based on tessellation factor: high tess = green, low = red
    float t = saturate((input.tessFactor - 1.0) / 15.0);
    float3 baseColor = lerp(float3(0.8, 0.2, 0.2), float3(0.2, 0.8, 0.3), t);

    // Add some height-based variation
    baseColor *= 0.7 + 0.3 * saturate(input.worldPos.y * 0.1 + 0.5);

    float3 lit = baseColor * (0.3 + 0.7 * NdotL);

    FSOutput output;
    output.color = float4(lit, 1.0);
    return output;
}

struct PSInput
{
    [[vk::location(0)]] float3 normal : NORMAL;
};

[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    float3 lightDir = normalize(float3(-1.0, -1.0, 0.0));
    float3 cubeColor = float3(1.0, 1.0, 1.0);

    float lightContrib = clamp(dot(input.normal, lightDir), 0.0, 1.0);
    float3 ambientLight = float3(0.1, 0.1, 0.1);

    float3 finalColor = (cubeColor * lightContrib) + ambientLight;
    return float4(finalColor, 1.0);
}

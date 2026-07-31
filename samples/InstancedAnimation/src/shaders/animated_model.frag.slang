struct PSInput
{
    [[vk::location(0)]] float3 fragNormal : NORMAL;
    [[vk::location(1)]] float2 fragUV     : TEXCOORD0;
};

[[vk::binding(0, 1)]] Sampler2D diffuseTexture;

[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    float3 lightDir = normalize(float3(0.5, 1.0, 0.3));
    float diff = max(dot(input.fragNormal, lightDir), 0.2);

    float4 texColor = diffuseTexture.Sample(input.fragUV);
    return float4(texColor.rgb * diff, texColor.a);
}

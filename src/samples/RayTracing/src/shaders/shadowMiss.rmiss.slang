struct RayPayload
{
    float3 hitColor;
    float shadowFactor;
    float3 bounceColor;
};

[shader("miss")]
void main(inout RayPayload payload)
{
    // Ray didn't hit anything — the point is not in shadow
    payload.shadowFactor = 1.0;
}

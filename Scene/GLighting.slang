public struct GLighting
{
    public float3 radiance;
    public float3 direction;

    public float3 evaluateIrradiance(float3 normal, float shadow = 1.0) { return radiance * max(0, dot(normal, direction)) * shadow; }
}

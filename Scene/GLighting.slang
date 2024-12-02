public struct GLighting
{
    public float3 radiance;
    public float3 direction;

    public float3 evaluateIrradiance(float3 normal) { return radiance * max(0, dot(normal, direction)); }
}

public struct GLighting
{
    // Sun radiance and direction
    public float3 radiance;
    public float3 direction;

    // Sky (environment) radiance
    public float3 skyRadiance;

    public float3 evaluateSunIrradiance(float3 normal, float shadow = 1.0) { return radiance * max(0, dot(normal, direction)) * shadow; }
}

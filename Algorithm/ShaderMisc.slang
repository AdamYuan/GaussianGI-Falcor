public float3 alignDirection(float3 n, float3 d)
{
    float3 u = normalize(cross(abs(n.x) > .01 ? float3(0, 1, 0) : float3(1, 0, 0), n));
    float3 v = cross(n, u);
    return d.x * u + d.y * v + d.z * n;
}

public bool alphaShouldDiscard(float alpha)
{
    return alpha < 0.5;
}

public float3 thermal(float x)
{
    return sin(clamp(x, 0.0, 1.0) * 3.0 - float3(1, 2, 3)) * 0.5 + 0.5;
}

[Differentiable]
public float3x3 getAlignRotMatT(float3 n)
{
    float3 y1 = cross(n, float3(0, 1, 0)), y2 = cross(n, float3(1, 0, 0));
    float3 u = normalize(cross(select(dot(y1, y1) > dot(y2, y2), y1, y2), n)); // Robust and continuous
    // float3 u = normalize(cross(abs(n.x) > .01 ? float3(0, 1, 0) : float3(1, 0, 0), n)); // Robust, but not continuous
    // float3 u = normalize(cross(float3(0, 1.1, 1.3), n)); // Continuous, but not robust
    float3 v = cross(n, u);
    return float3x3(u, v, n);
}

[Differentiable]
public float3x3 getAlignRotMat(float3 n)
{
    return transpose(getAlignRotMatT(n));
}

[Differentiable]
public float3 alignDirection(float3 n, float3 d)
{
    return mul(getAlignRotMat(n), d);
}

public bool alphaShouldDiscard(float alpha)
{
    return alpha < 0.5;
}

public float3 thermal(float x)
{
    return sin(clamp(x, 0.0, 1.0) * 3.0 - float3(1, 2, 3)) * 0.5 + 0.5;
}

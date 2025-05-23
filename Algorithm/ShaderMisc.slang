[Differentiable]
public float3x3 getAlignRotMatT(float3 n)
{
    // float3 u = normalize(cross(abs(n.x) > .01 ? float3(0, 1, 0) : float3(1, 0, 0), n)); // Robust, but not continuous
    float3 u = normalize(cross(float3(0, 1.1, 1.3), n)); // Continuous, but not robust
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

[Differentiable]
public float3x3 getRobustAlignRotMatT(float3 n)
{
    float3 u = normalize(cross(abs(n.x) > .01 ? float3(0, 1, 0) : float3(1, 0, 0), n));
    float3 v = cross(n, u);
    return float3x3(u, v, n);
}

[Differentiable]
public float3x3 getRobustAlignRotMat(float3 n)
{
    return transpose(getRobustAlignRotMatT(n));
}

[Differentiable]
public float3 alignDirectionRobust(float3 n, float3 d)
{
    return mul(getRobustAlignRotMat(n), d);
}

public bool alphaShouldDiscard(float alpha)
{
    return alpha < 0.5;
}

public float3 thermal(float x)
{
    return sin(clamp(x, 0.0, 1.0) * 3.0 - float3(1, 2, 3)) * 0.5 + 0.5;
}

// Radix Tricks by Michael Herf
// http://stereopsis.com/radix.html
public uint float2CmpUint(float f)
{
    uint mask = -((int)(asuint(f) >> 31)) | 0x80000000;
    return asuint(f) ^ mask;
}

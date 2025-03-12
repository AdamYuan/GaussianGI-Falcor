public struct GTransform
{
    public float3 center;
    public uint packedScaleRotateY;

    public property float scale
    {
        get {
            return f16tof32(packedScaleRotateY);
        }
    }

    public property float rotateY
    {
        get {
            return f16tof32(packedScaleRotateY >> 16u);
        }
    }

    public float3 applyRotation(float3 p)
    {
        float cosRotY = cos(float(rotateY)), sinRotY = sin(float(rotateY));

        // [ cosR  -sinR ]
        // [ sinR   cosR ]
        float2 pXZ = {
            cosRotY * p.x - sinRotY * p.z,
            sinRotY * p.x + cosRotY * p.z,
        };
        p.x = pXZ[0];
        p.z = pXZ[1];
        return p;
    }

    public float4x4 getMatrix()
    {
        float cosRotY = cos(float(rotateY)), sinRotY = sin(float(rotateY));

        return float4x4(
            float4(scale * cosRotY, 0, -scale * sinRotY, center.x),
            float4(0, scale, 0, center.y),
            float4(scale * sinRotY, 0, scale * cosRotY, center.z),
            float4(0, 0, 0, 1)
        );
    }

    public float3x3 getRotateMatrix()
    {
        float cosRotY = cos(float(rotateY)), sinRotY = sin(float(rotateY));

        return float3x3(
            float3(cosRotY, 0, -sinRotY), //
            float3(0, 1, 0),
            float3(sinRotY, 0, cosRotY),
        );
    }

    public float4 getRotateQuat()
    {
        float halfRotY = rotateY * 0.5;
        float cosHalfRotY = cos(halfRotY), sinHalfRotY = sin(halfRotY);
        return float4(0, -sinHalfRotY, 0, cosHalfRotY);
    }

    public float3 apply(float3 p)
    {
        p = applyRotation(p);
        p *= this.scale;
        p += this.center;
        return p;
    }
}

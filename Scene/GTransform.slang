public struct GTransform
{
    public float3 center;
    public float scale;
    public float cosRotateY, sinRotateY;

    public float3 applyRotation(float3 p)
    {
        // [ cosR  -sinR ]
        // [ sinR   cosR ]
        float2 pXZ = {
            this.cosRotateY * p.x - this.sinRotateY * p.z,
            this.sinRotateY * p.x + this.cosRotateY * p.z,
        };
        p.x = pXZ[0];
        p.z = pXZ[1];
        return p;
    }

    public float4x4 getMatrix()
    {
        return float4x4(
            float4(scale * this.cosRotateY, 0, -scale * this.sinRotateY, center.x),
            float4(0, scale, 0, center.y),
            float4(scale * this.sinRotateY, 0, scale * this.cosRotateY, center.z),
            float4(0, 0, 0, 1)
        );
    }

    public float3x3 getRotateMatrix()
    {
        return float3x3(
            float3(this.cosRotateY, 0, -this.sinRotateY), //
            float3(0, 1, 0),
            float3(this.sinRotateY, 0, this.cosRotateY),
        );
    }

    public float3 apply(float3 p)
    {
        p = applyRotation(p);
        p *= this.scale;
        p += this.center;
        return p;
    }
}

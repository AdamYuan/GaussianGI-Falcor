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

    public float3 apply(float3 p)
    {
        p = applyRotation(p);
        p *= this.scale;
        p += this.center;
        return p;
    }
}

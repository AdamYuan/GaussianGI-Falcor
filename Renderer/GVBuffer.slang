public struct GVBuffer
{
    public struct Texel
    {
        public float3 albedo;
        public bool isHit;
        public bool isFrontFace;
        public uint meshPrimitiveID;
        public uint meshID;
        public uint instanceID;
        public float2 barycentrics;
    }

    public uint2 resolution;
    Texture2D<float4> albedoTexture;
    Texture2D<uint4> primitiveTexture;

    public Texel Load(uint2 coord)
    {
        Texel t = (Texel)0;
        if (any(coord >= resolution))
            return t;

        float4 albedoTexel = albedoTexture.Load(int3(coord, 0));
        uint4 primitiveTexel = primitiveTexture.Load(int3(coord, 0));

        t.albedo = albedoTexel.rgb;
        t.isHit = primitiveTexel.x != 0xFFFFFFFF;
        t.isFrontFace = (primitiveTexel.x & 0x80000000) == 0;
        t.meshPrimitiveID = primitiveTexel.x & 0x7FFFFFFF;
        t.meshID = primitiveTexel.y >> 16;
        t.instanceID = primitiveTexel.y & 0xFFFF;
        t.barycentrics = asfloat(primitiveTexel.zw);

        return t;
    }
}

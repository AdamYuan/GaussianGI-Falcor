import GaussianGI.Scene.GStaticScene;

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
    Texture2D<uint4> hitTexture;

    public float3 LoadAlbedo(uint2 coord) { return albedoTexture.Load(int3(coord, 0)).rgb; }
    public GStaticScene::HitInfo LoadHitInfo(uint2 coord)
    {
        var t = (GStaticScene::HitInfo)0;
        if (any(coord >= resolution))
            return t;

        uint4 texel = hitTexture.Load(int3(coord, 0));

        t.isHit = texel.x != 0xFFFFFFFF;
        t.isFrontFace = (texel.x & 0x80000000) == 0;
        t.meshPrimitiveID = texel.x & 0x7FFFFFFF;
        t.meshID = texel.y >> 16;
        t.instanceID = texel.y & 0xFFFF;
        t.barycentrics = asfloat(texel.zw);

        return t;
    }
}

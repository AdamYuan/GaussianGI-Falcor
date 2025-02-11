import GaussianGI.Scene.GStaticScene;
import Utils.Math.Quaternion;

public namespace GS3DIndLight
{
    public struct Splat
    {
        public float3 mean;
        public float16_t4 rotate;
        public float16_t3 scale;
        public float16_t3 indirect;
    }

    public struct SplatView
    {
        public float16_t2 axis0, axis1;
        public float16_t2 clipXY;
        uint padding;
    }

    public struct MiscSplatView
    {
        public float16_t2 axis0, axis1;
        public float16_t2 clipXY;
        uint padding;
    }

    public struct InstancedSplatBuffer
    {
        StructuredBuffer<Splat> splats;
        StructuredBuffer<uint> splatDescs;
        public uint splatCount;

        public Splat load(uint splatIdx)
        {
            uint splatDesc = splatDescs[splatIdx];
            uint realSplatIdx = splatDesc & 0xFFFFFFu;
            uint instanceID = splatDesc >> 24u;
            var transform = gGStaticScene.getInstanceInfo(instanceID).transform;

            Splat splat = splats[realSplatIdx];
            splat.mean = transform.apply(splat.mean);
            splat.scale *= float16_t(transform.scale);
            splat.rotate = float16_t4(qmul(transform.getRotateQuat(), float4(splat.rotate)));

            return splat;
        }
    }
}

import GaussianGI.Scene.GStaticScene;
import Utils.Math.Quaternion;
import Utils.Math.FormatConversion;

public namespace GS3DIndLight
{
    public struct SplatGeom
    {
        public float3 mean;
        public float4 rotate;
        public float3 scale;
        public float16_t3 indirect;
    }

    public struct SplatAttrib
    {
        public float3 albedo;
    }

    public struct Splat
    {
        public SplatGeom geom;
        public SplatAttrib attrib;
    }

    float3 unpackUnorm3x10(uint packed)
    {
        return float3(uint3(packed, packed >> 10, packed >> 20) & 1023) * (1.f / 1023);
    }

    public struct PackedSplatGeom
    {
        public uint rotate;
        public uint meanXY;
        public uint16_t meanZ;
        public float16_t3 scale;

        public SplatGeom unpack()
        {
            SplatGeom geom;
            geom.rotate.xyz = unpackUnorm3x10(rotate) * 2.0 - 1.0;
            geom.rotate.w = sqrt(1.0 - dot(geom.rotate.xyz, geom.rotate.xyz));
            geom.mean.xy = unpackSnorm2x16(meanXY);
            geom.mean.z = unpackSnorm16(meanZ);
            geom.scale = scale;
            return geom;
        }
    }

    public struct PackedSplatAttrib
    {
        public uint albedo;

        public SplatAttrib unpack()
        {
            SplatAttrib attrib;
            attrib.albedo = unpackUnorm3x10(albedo);
            return attrib;
        }
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

    public struct InstanceDesc
    {
        uint firstSplatIdx;
    }

    public struct InstancedSplatBuffer
    {
        public StructuredBuffer<PackedSplatGeom> splatGeoms;
        public StructuredBuffer<PackedSplatAttrib> splatAttribs;
        public StructuredBuffer<uint> splatDescs;
        public StructuredBuffer<InstanceDesc> instanceDescs;
        public uint splatCount;

        public Splat load(uint meshSplatIdx, uint instanceID)
        {
            var transform = gGStaticScene.getInstanceInfo(instanceID).transform;
            Splat splat;
            splat.geom = splatGeoms[meshSplatIdx].unpack();
            splat.geom.mean = transform.apply(splat.geom.mean);
            splat.geom.scale *= transform.scale;
            splat.geom.rotate = float4(qmul(transform.getRotateQuat(), float4(splat.geom.rotate)));
            splat.attrib = splatAttribs[meshSplatIdx].unpack();
            return splat;
        }
        public Splat load(uint splatIdx)
        {
            uint splatDesc = splatDescs[splatIdx];
            uint meshSplatIdx = splatDesc & 0xFFFFFFu;
            uint instanceID = splatDesc >> 24u;
            return load(meshSplatIdx, instanceID);
        }

        public InstanceDesc loadInstanceDesc(uint instanceID) { return instanceDescs[instanceID]; }
    }
}

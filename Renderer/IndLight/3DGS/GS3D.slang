import Utils.Math.Quaternion;
import GaussianGI.Scene.GTransform;
__exported import GaussianGI.Algorithm.GS3D;

public namespace GS3D
{
    public struct PackedSplat
    {
        public float3 position;
        public float16_t4 rotate;
        public float16_t2 scaleXY;
    }

    public struct PackedSplatView
    {
        public float16_t2 axis0, axis1;
        public float16_t2 clipXY;
        public uint color; // temporally use RGBA8, should use HDR formats later
    }
}

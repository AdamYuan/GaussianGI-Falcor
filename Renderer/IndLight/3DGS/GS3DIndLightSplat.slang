public struct IndLightSplat
{
    public float3 mean;
    public float16_t4 rotate;
    public float16_t3 scale;
    public float16_t3 indirect;
}

public struct IndLightSplatView
{
    public float16_t2 axis0, axis1;
    public float16_t2 clipXY;
    public float16_t4 indirect;
}

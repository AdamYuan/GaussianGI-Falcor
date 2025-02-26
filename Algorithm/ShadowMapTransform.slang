public struct ShadowMapTransform
{
    float3x4 lightMat;

    property float3 negCenter
    {
        get {
            return float3(lightMat[0].w, lightMat[1].w, lightMat[2].w);
        }
    };
    property float3x3 rotMat
    {
        get {
            return float3x3(lightMat[0].xyz, lightMat[1].xyz, lightMat[2].xyz);
        }
    };

    public float3 getClipPos(float3 worldPos)
    {
        float3 clipPos = mul(rotMat, worldPos + negCenter);
        clipPos.z = clipPos.z * 0.5 + 0.5;
        return clipPos;
    }

    public float3 getUVDepth(float3 worldPos)
    {
        float3 uvDepth = mul(rotMat, worldPos + negCenter);
        uvDepth = uvDepth * 0.5 + 0.5;
        uvDepth.y = 1.0 - uvDepth.y;
        return uvDepth;
    }
}

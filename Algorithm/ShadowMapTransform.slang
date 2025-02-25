public struct ShadowMapTransform
{
    float3x4 lightMat;

    public float3 getClipPos(float3 worldPos)
    {
        float3 clipPos = mul(lightMat, float4(worldPos, 1.0));
        clipPos.z = clipPos.z * 0.5 + 0.5;
        return clipPos;
    }

    public float3 getUVDepth(float3 worldPos)
    {
        float3 uvDepth = mul(lightMat, float4(worldPos, 1.0));
        uvDepth = uvDepth * 0.5 + 0.5;
        return uvDepth;
    }
}

#include "Utils/Math/MathConstants.slangh"
#include "EVSM.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.ShadowMapTransform;

public struct EVSMShadow
{
    Texture2D<float4> shadowMap;
    SamplerState smSampler;
    ShadowMapTransform smTransform;

    public float evaluate(in const GStaticScene staticScene, in const GStaticScene::HitPoint hitPoint)
    {
        float3 lightDir = staticScene.lighting.direction;
        if (dot(lightDir, hitPoint.geomNormal) <= 0)
            return 0.0;
        float3 uvDepth = smTransform.getUVDepth(hitPoint.position);
        float2 warpedDepth = warpDepth(uvDepth.z);
        float4 occluder = shadowMap.SampleLevel(smSampler, uvDepth.xy, 0);
        float posContrib = chebyshevUpperBound(occluder.xz, warpedDepth.x, 0.0);
        float negContrib = chebyshevUpperBound(occluder.yw, warpedDepth.y, 0.0);
        return min(posContrib, negContrib);
    }
}

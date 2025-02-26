#include "Utils/Math/MathConstants.slangh"
#include "VSM.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.ShadowMapTransform;

public struct VSMShadow
{
    Texture2D<float4> shadowMap;
    SamplerState smSampler;
    ShadowMapTransform smTransform;
    float vsmBias;

    public float evaluate(in const GStaticScene staticScene, in const GStaticScene::HitPoint hitPoint)
    {
        /* float3 lightDir = staticScene.lighting.direction;
        if (dot(lightDir, hitPoint.geomNormal) <= 0)
            return 0.0; */

        float3 uvDepth = smTransform.getUVDepth(hitPoint.position);
        float2 moment = shadowMap.SampleLevel(smSampler, uvDepth.xy, 0).xy;
        return chebyshevUpperBound(moment, uvDepth.z, vsmBias * 0.01);
    }
}

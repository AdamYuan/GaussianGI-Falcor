#include "Utils/Math/MathConstants.slangh"
import GaussianGI.Scene.GStaticScene;

public struct RTShadow
{
    public static float evaluate(in const GStaticScene staticScene, in const GStaticScene::HitPoint hitPoint)
    {
        float3 lightDir = staticScene.lighting.direction;
        if (dot(lightDir, hitPoint.normal) <= 0 || dot(lightDir, hitPoint.geomNormal) <= 0)
            return 0.0;

        Ray shadowRay;
        shadowRay.tMin = 0.0;
        shadowRay.tMax = FLT_MAX;
        shadowRay.origin = hitPoint.getRayOrigin();
        shadowRay.dir = lightDir;
        return gGStaticScene.traceAnyHit(shadowRay, true) ? 0.0 : 1.0;
    }
}

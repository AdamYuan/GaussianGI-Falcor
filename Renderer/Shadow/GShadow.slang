import RT.RTShadow;
import VSM.VSMShadow;
import GaussianGI.Scene.GStaticScene;

#ifndef GSHADOW_IDENTIFIER
#define GSHADOW_IDENTIFIER noShadow
#endif

public struct GShadow
{
    struct NoShadow
    {
        static float evaluate(in const GStaticScene staticScene, in const GStaticScene::HitPoint hitPoint) { return 1.0; }
        static float evaluate(in const GStaticScene staticScene, float3 worldPos) { return 1.0; }
    }

    NoShadow noShadow;
    RTShadow rtShadow;
    NoShadow pcfShadow;
    VSMShadow vsmShadow;

    public float evaluate(in const GStaticScene staticScene, in const GStaticScene::HitPoint hitPoint)
    {
        return GSHADOW_IDENTIFIER.evaluate(staticScene, hitPoint);
    }
    public float evaluate(in const GStaticScene staticScene, float3 worldPos) { return GSHADOW_IDENTIFIER.evaluate(staticScene, worldPos); }
}

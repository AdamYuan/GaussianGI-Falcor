import RT.RTShadow;
import GaussianGI.Scene.GStaticScene;

#ifndef GSHADOW_IDENTIFIER
#define GSHADOW_IDENTIFIER noShadow
#endif

public struct GShadow
{
    struct NoShadow
    {
        static float evaluate(in const GStaticScene staticScene, in const GStaticScene::HitPoint hitPoint) { return 1.0; }
    }

    NoShadow noShadow;
    RTShadow rtShadow;
    NoShadow pcfShadow;
    NoShadow evsmShadow;

    public float evaluate(in const GStaticScene staticScene, in const GStaticScene::HitPoint hitPoint)
    {
        return GSHADOW_IDENTIFIER.evaluate(staticScene, hitPoint);
    }
}

import RT.RTShadow;
import EVSM.EVSMShadow;
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
    EVSMShadow evsmShadow;

    public float evaluate(in const GStaticScene staticScene, in const GStaticScene::HitPoint hitPoint)
    {
        return GSHADOW_IDENTIFIER.evaluate(staticScene, hitPoint);
    }
}

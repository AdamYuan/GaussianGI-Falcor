import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.GS3D;
import GaussianGI.Renderer.Shadow.GShadow;
import GS3DIndLightSplat;

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
uniform GShadow gGShadow;

RWBuffer<float> gSplatShadows;

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatID = threadID.x;
    if (splatID >= gSplats.splatCount)
        return;

    var splatGeom = gSplats.load(splatID).geom;
    gSplatShadows[splatID] = gGShadow.evaluate(gGStaticScene, splatGeom.mean);
}

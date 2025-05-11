import GaussianGI.Scene.GStaticScene;
import Utils.Math.FormatConversion;
import GaussianGI.Algorithm.GS3D;
import GS3DIndLightSplat;

#include "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightSplatPrimitive.slangh"

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
uniform float2 gResolution;

RWStructuredBuffer<uint> gSplatDrawArgs;
RWStructuredBuffer<uint> gSplatIDs;

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatID = threadID.x;
    if (splatID >= gSplats.splatCount)
        return;

    var splatGeom = gSplats.load(splatID).geom;

    GSPrimitive gsPrim = GSPrimitive::create(splatGeom, gGStaticScene.camera, gResolution);

    if (gsPrim.isFrustumCulled())
        return;

    uint viewID;
    InterlockedAdd(gSplatDrawArgs[1], 1, viewID); // gSplatDrawArgs[1] is instanceCount
    gSplatIDs[viewID] = splatID;
}

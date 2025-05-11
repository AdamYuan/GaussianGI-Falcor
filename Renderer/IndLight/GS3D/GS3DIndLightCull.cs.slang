import GaussianGI.Scene.GStaticScene;
import Utils.Math.FormatConversion;
import GaussianGI.Algorithm.GS3D;
import GS3DIndLightSplat;

#include "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightSplatPrimitive.slangh"

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
uniform float2 gResolution;

#ifndef USE_DEPTH_GSPP
#define USE_DEPTH_GSPP 1
#endif

RWStructuredBuffer<uint> gSplatDrawArgs, gDepthGSPPSplatDrawArgs;
RWStructuredBuffer<uint> gSplatIDs;

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatID = threadID.x;
    if (splatID >= gSplats.splatCount)
        return;

    var splatGeom = gSplats.load(splatID).geom;

#if USE_DEPTH_GSPP
    float3 splatCamMean = splatGeom.mean - gGStaticScene.camera.getPosition();
    float splatCamDist = length(splatCamMean);
    float splatMaxDist = max(splatGeom.scale.x, max(splatGeom.scale.y, splatGeom.scale.z)) * BOUND_FACTOR;
    bool depthPass = splatCamDist > splatMaxDist + 1e-3;
    GSPPPrimitive gsPrim =
        GSPPPrimitive::create(splatGeom, gGStaticScene.camera, gResolution, depthPass ? (splatCamDist - splatMaxDist) : 1.0);
#else
    GSPrimitive gsPrim = GSPrimitive::create(splatGeom, gGStaticScene.camera, gResolution);
#endif

    if (gsPrim.isFrustumCulled())
        return;

#if USE_DEPTH_GSPP
    uint viewID;
    if (depthPass)
        InterlockedAdd(gDepthGSPPSplatDrawArgs[1], 1, viewID); // gSplatDrawArgs[1] is instanceCount
    else
        InterlockedAdd(gSplatDrawArgs[1], 1, viewID); // gSplatDrawArgs[1] is instanceCount

    if (depthPass)
        gSplatIDs[gSplats.splatCount - viewID - 1] = splatID;
    else
        gSplatIDs[viewID] = splatID;
#else
    uint viewID;
    InterlockedAdd(gSplatDrawArgs[1], 1, viewID); // gSplatDrawArgs[1] is instanceCount
    gSplatIDs[viewID] = splatID;
#endif
}

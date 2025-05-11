import GaussianGI.Scene.GStaticScene;
import Utils.Math.FormatConversion;
import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Renderer.IndLight.GS3D.GS3DIndLightSplat;
#include "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightSplatPrimitive.slangh"

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
uniform float2 gResolution;

RWStructuredBuffer<uint> gSplatViewDrawArgs;
RWStructuredBuffer<uint> gSplatViewSortKeys;
RWStructuredBuffer<uint> gSplatViewSortPayloads;

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

    float viewPosZ = mul(gGStaticScene.camera.data.viewMat, float4(splatGeom.mean, 1)).z;

    uint viewID;
    InterlockedAdd(gSplatViewDrawArgs[1], 1, viewID); // gSplatViewDrawArgs[1] is instanceCount
    gSplatViewSortKeys[viewID] = float2CmpUint(viewPosZ);
    gSplatViewSortPayloads[viewID] = splatID;
}

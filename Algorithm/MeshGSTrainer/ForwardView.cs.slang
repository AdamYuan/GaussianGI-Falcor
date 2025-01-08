#include "Trainer.slangh"
import Scene.Camera.Camera;
import Utils.Math.FormatConversion;
import GaussianGI.Algorithm.GS3D;

uniform uint gSplatCount;
SplatBuffer gSplats;

RWStructuredBuffer<uint> gSplatViewCount;
RWSplatViewBuffer gSplatViews;
RWStructuredBuffer<uint> gSplatViewSplatID;
RWStructuredBuffer<uint> gSplatViewSortKeys;
RWStructuredBuffer<uint> gSplatViewSortPayloads;

uniform float2 gCamResolution;
uniform Camera gCamera;

// Radix Tricks by Michael Herf
// http://stereopsis.com/radix.html
uint float2Uint(float f)
{
    uint mask = -((int)(asuint(f) >> 31)) | 0x80000000;
    return asuint(f) ^ mask;
}

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatID = threadID.x;
    if (splatID >= gSplatCount)
        return;

    Splat splat = gSplats.load(splatID);

    float3 viewPos = mul(gCamera.data.viewMat, float4(splat.mean, 1)).xyz;
    float4 clipPos = mul(gCamera.data.projMat, float4(viewPos, 1));
    if (clipPos.w <= 0) // Behind Camera
        return;
    clipPos /= clipPos.w;
    if (any(abs(clipPos.xy) > 1.3) || clipPos.z > 1 || clipPos.z < 0) // Frustum Culled
        return;

    SplatView splatView = splat2splatView(
        splat, //
        viewPos,
        clipPos.xyz,
        gCamera.data.viewMat,
        gCamera.data.projMat,
        gCamera.data.nearZ,
        gCamera.data.farZ,
        gCamResolution
    );

    uint splatViewID;
    InterlockedAdd(gSplatViewCount[0], 1, splatViewID);
    gSplatViewSplatID[splatViewID] = splatID;
    gSplatViewSortPayloads[splatViewID] = splatViewID;
    gSplatViewSortKeys[splatViewID] = float2Uint(viewPos.z);
    gSplatViews.store(splatViewID, splatView);
}

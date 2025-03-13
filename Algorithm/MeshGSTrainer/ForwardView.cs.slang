import GaussianGI.Algorithm.GS3D;
import MeshGSTrainer;

using namespace MeshGSTrain;

uniform Camera gCamera;

uniform uint gSplatCount;
SplatBuffer gSplats;

RWStructuredBuffer<uint> gSplatViewDrawArgs; // drawArgs[1] is instanceCount also splatViewCount
RWSplatViewBuffer gSplatViews;
// RWSplatViewBuffer gDLossDSplatViews;
RWStructuredBuffer<uint> gSplatViewSplatIDs;
RWStructuredBuffer<uint> gSplatViewSortKeys;
RWStructuredBuffer<uint> gSplatViewSortPayloads;
RWSplatQuadBuffer gSplatQuads;

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

    var splat = gSplats.load<Splat>(splatID);

    float3 viewPos = mul(gCamera.viewMat, float4(splat.geom.mean, 1)).xyz;
    float4 clipPos = mul(gCamera.projMat, float4(viewPos, 1));
    if (clipPos.w <= 0) // Behind Camera
        return;
    clipPos /= clipPos.w;
    if (any(abs(clipPos.xy) > 1.3) || clipPos.z > 1 || clipPos.z < 0) // Frustum Culled
        return;

    SplatQuad splatQuad;

    SplatView splatView = splat2splatView(
        splat, //
        viewPos,
        clipPos.xyz,
        splatQuad,
        gCamera.viewMat,
        gCamera.projMat,
        gCamera.nearZ,
        gCamera.farZ
    );

    uint splatViewID;
    InterlockedAdd(gSplatViewDrawArgs[1], 1, splatViewID);
    gSplatViewSplatIDs[splatViewID] = splatID;
    gSplatViewSortPayloads[splatViewID] = splatViewID;
    gSplatViewSortKeys[splatViewID] = float2Uint(viewPos.z);
    gSplatQuads.store(splatViewID, splatQuad);
    gSplatViews.store(splatViewID, splatView);
    // gDLossDSplatViews.store(splatViewID, (SplatView)0); // Clear DLoss
}

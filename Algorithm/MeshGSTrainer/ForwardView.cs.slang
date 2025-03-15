import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.FrustumCull;
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

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatID = threadID.x;
    if (splatID >= gSplatCount)
        return;

    var splat = gSplats.load<Splat>(splatID);

    float3 viewPos = mul(gCamera.viewMat, float4(splat.geom.mean, 1)).xyz;
    float4 clipPos = mul(gCamera.projMat, float4(viewPos, 1));
    /* if (clipPos.w <= 0) // Behind Camera
        return;

    float3 clipPos3 = clipPos.xyz / clipPos.w;
    if (any(abs(clipPos3.xy) > 1.3) || clipPos3.z > 1 || clipPos3.z < 0) // Frustum Culled
        return; */

    SplatQuad splatQuad;

    SplatView splatView = splat2splatView(
        splat, //
        viewPos,
        clipPos.xyz / clipPos.w,
        splatQuad,
        gCamera.viewMat,
        gCamera.projMat,
        gCamera.nearZ,
        gCamera.farZ
    );

    if (splatView.toSOA().anyIsNanOrInf())
        return;

    float4 deltaClip0 = mul(gCamera.projMat, float4(splatQuad.deltaView0, 0)) * kQuadPosBound;
    float4 deltaClip1 = mul(gCamera.projMat, float4(splatQuad.deltaView1, 0)) * kQuadPosBound;

    float4 quadClips[4];
    quadClips[0] = clipPos - deltaClip0 - deltaClip1;
    quadClips[1] = clipPos - deltaClip0 + deltaClip1;
    quadClips[2] = clipPos + deltaClip0 + deltaClip1;
    quadClips[3] = clipPos + deltaClip0 - deltaClip1;

    if (cullQuadClip(quadClips))
        return;

    uint splatViewID;
    InterlockedAdd(gSplatViewDrawArgs[1], 1, splatViewID);
    gSplatViewSplatIDs[splatViewID] = splatID;
    gSplatViewSortPayloads[splatViewID] = splatViewID;
    gSplatViewSortKeys[splatViewID] = float2CmpUint(viewPos.z);
    gSplatQuads.store(splatViewID, splatQuad);
    gSplatViews.store(splatViewID, splatView);
    // gDLossDSplatViews.store(splatViewID, (SplatView)0); // Clear DLoss
}

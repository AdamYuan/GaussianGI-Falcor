import GaussianGI.Scene.GStaticScene;
import Utils.Math.FormatConversion;
import GaussianGI.Algorithm.GS3D;
import GS3DIndLightSplat;

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
uniform float2 gResolution;

RWStructuredBuffer<SplatView> gSplatViews;
RWStructuredBuffer<uint> gSplatViewDrawArgs;
RWStructuredBuffer<uint> gSplatViewSortKeys;
RWStructuredBuffer<uint> gSplatViewSortPayloads;

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
    if (splatID >= gSplats.splatCount)
        return;

    var splat = gSplats.load(splatID);

    float3 viewPos = mul(gGStaticScene.camera.data.viewMat, float4(splat.mean, 1)).xyz;
    float4 clipPos = mul(gGStaticScene.camera.data.projMat, float4(viewPos, 1));
    if (clipPos.w <= 0) // Behind Camera
        return;
    clipPos /= clipPos.w;
    if (any(abs(clipPos.xy) > 1.3) || clipPos.z > 1 || clipPos.z < 0) // Frustum Culled
        return;

    float2x3 cov3d = GS3D::getCovariance3D(splat.rotate, float3(splat.scale));
    float3 cov2d = GS3D::getCovariance2D(
        cov3d, viewPos, float3x3(gGStaticScene.camera.data.viewMat), gGStaticScene.camera.data.projMat[0][0], gResolution
    );
    GS3D::Eigen<2> eigen2d = GS3D::decomposeCovariance2D(cov2d);

    float2 axis0 = eigen2d.eigen[0] * sqrt(2.0 * eigen2d.lambda[0]);
    float2 axis1 = eigen2d.eigen[1] * sqrt(2.0 * eigen2d.lambda[1]);
    // e^{-d^2} = e^{-\frac{1}{2}(\frac{a}{\sigma})^2} \Rightarrow a = \sqrt{2}\sigma d
    // Let a = 2\sqrt{2} \sigma < 3 \sigma, then d = 2
    SplatView view;
    view.axis0 = float16_t2(axis0);
    view.axis1 = float16_t2(axis1);
    view.clipXY = float16_t2(clipPos.xy);

    uint viewID;
    InterlockedAdd(gSplatViewDrawArgs[1], 1, viewID); // gSplatViewDrawArgs[1] is instanceCount
    gSplatViews[splatID] = view;
    gSplatViewSortKeys[viewID] = float2Uint(viewPos.z);
    gSplatViewSortPayloads[viewID] = splatID;
}

#include "GaussianGI/Algorithm/GS3DBound.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.GS3D;
import GaussianGI.Renderer.Shadow.GShadow;
import GS3DIndLightSplat;
import Utils.Math.Quaternion;

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
    float3x3 splatRotMat = GS3D::getQuatMatrix(splatGeom.rotate);
    float3x3 splatScaleMat = GS3D::getScaleMatrix(splatGeom.scale) * GS3D_BOUND_SQRT_2LOG_255;
    float3x3 splatMat = mul(splatRotMat, splatScaleMat);
    float3x3 splatMatT = transpose(splatMat);

    float3 rotT_V = mul(splatMatT, gGStaticScene.lighting.direction);
    float3 evalWorldPos = mul(splatMat, normalize(rotT_V)) + splatGeom.mean;

    gSplatShadows[splatID] = gGShadow.evaluate(gGStaticScene, evalWorldPos);
}

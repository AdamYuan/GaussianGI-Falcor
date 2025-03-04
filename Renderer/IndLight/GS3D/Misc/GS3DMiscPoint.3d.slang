#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Algorithm.SphericalHarmonics;
import GaussianGI.Renderer.IndLight.GS3D.GS3DIndLightSplat;
import GS3DMiscColor;

using namespace GS3DIndLight;

StructuredBuffer<uint> gSplatDescs;
uniform InstancedSplatBuffer gSplats;
uniform uint gSplatCount;

Buffer<float> gSplatShadows;
StructuredBuffer<SH<float3, 2>> gSplatProbes;

struct PSIn
{
    linear float4 clipPos : SV_Position;
    nointerpolation float3 color;
}

PSIn vsMain(uint vertexID: SV_VertexID)
{
    var splat = gSplats.load(vertexID);
    float viewZ = mul(gGStaticScene.camera.data.viewMat, float4(splat.geom.mean, 1.0)).z;
    float viewZRemap = -viewZ / gGStaticScene.camera.data.farZ;

    PSIn o;
    o.clipPos = mul(gGStaticScene.camera.getViewProj(), float4(splat.geom.mean, 1));
    o.color = getGS3DMiscColor(splat, gSplatShadows[vertexID], gSplatProbes[vertexID]);
    return o;
}

float4 psMain(PSIn pIn)
{
    return float4(pIn.color, 1);
}

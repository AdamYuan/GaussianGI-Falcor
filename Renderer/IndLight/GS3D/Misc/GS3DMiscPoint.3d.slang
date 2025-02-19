#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Renderer.IndLight.GS3D.GS3DIndLightSplat;

using namespace GS3DIndLight;

StructuredBuffer<uint> gSplatDescs;
uniform InstancedSplatBuffer gSplats;
uniform uint gSplatCount;

struct PSIn
{
    linear float4 clipPos : SV_Position;
    nointerpolation float16_t3 albedo;
}

PSIn vsMain(uint vertexID: SV_VertexID)
{
    var splat = gSplats.load(vertexID);
    float viewZ = mul(gGStaticScene.camera.data.viewMat, float4(splat.geom.mean, 1.0)).z;
    float viewZRemap = -viewZ / gGStaticScene.camera.data.farZ;

    PSIn o;
    o.clipPos = mul(gGStaticScene.camera.getViewProj(), float4(splat.geom.mean, 1));
    o.albedo = float16_t3(splat.attrib.albedo);
    return o;
}

float4 psMain(PSIn pIn)
{
    return float4(float3(pIn.albedo), 1);
}

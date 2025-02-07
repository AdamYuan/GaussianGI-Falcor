#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.ShaderMisc;
import GS3DIndLightSplat;

StructuredBuffer<IndLightSplat> gSplats;
uniform uint gSplatsPerMesh;

struct PSIn
{
    linear float4 clipPos : SV_Position;
    nointerpolation float16_t3 indirect;
}

PSIn vsMain(uint vertexID: SV_VertexID)
{
    uint instanceID = vertexID / gSplatsPerMesh;
    var instanceInfo = gGStaticScene.getInstanceInfo(instanceID);

    var splat = gSplats[instanceInfo.meshID * gSplatsPerMesh + vertexID % gSplatsPerMesh];
    float3 position = instanceInfo.transform.apply(splat.mean);
    float viewZ = mul(gGStaticScene.camera.data.viewMat, float4(position, 1.0)).z;
    float viewZRemap = -viewZ / gGStaticScene.camera.data.farZ;

    PSIn o;
    o.clipPos = mul(gGStaticScene.camera.getViewProj(), float4(position, 1));
    o.indirect = splat.indirect;
    return o;
}

float4 psMain(PSIn pIn)
{
    return float4(float3(pIn.indirect), 1);
}

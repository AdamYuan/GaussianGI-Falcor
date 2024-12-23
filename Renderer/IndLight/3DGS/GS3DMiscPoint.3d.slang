#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Util.ShaderUtil;
import GS3D;

StructuredBuffer<GS3D::Splat> gSplats;
uniform uint gSplatsPerMesh;

struct PSIn
{
    linear float4 clipPos : SV_Position;
    nointerpolation float3 texColor;
}

PSIn vsMain(uint vertexID: SV_VertexID)
{
    uint instanceID = vertexID / gSplatsPerMesh;
    var instanceInfo = gGStaticScene.getInstanceInfo(instanceID);

    GS3D::Splat splat = gSplats[instanceInfo.meshID * gSplatsPerMesh + vertexID % gSplatsPerMesh];

    var hitInfo = (GStaticScene::HitInfo)0;
    hitInfo.isHit = true;
    hitInfo.isFrontFace = true;
    hitInfo.instanceID = instanceID;
    hitInfo.meshPrimitiveID = splat.primitiveID;
    hitInfo.barycentrics = splat.barycentrics;

    var hitPoint = gGStaticScene.getHitPoint(hitInfo, instanceInfo);

    PSIn o;
    o.clipPos = mul(gGStaticScene.camera.getViewProj(), float4(hitPoint.position, 1));
    o.texColor = gGStaticScene.textures[hitPoint.textureID].SampleLevel(gGStaticScene.sampler, hitPoint.texcoord, 0).rgb;
    return o;
}

float4 psMain(PSIn pIn)
{
    return float4(pIn.texColor, 1);
}

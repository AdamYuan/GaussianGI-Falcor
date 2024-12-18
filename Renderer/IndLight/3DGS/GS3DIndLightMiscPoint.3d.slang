#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Util.ShaderUtil;
import GS3DIndLightMisc;

struct PSIn
{
    linear float4 clipPos : SV_Position;
    nointerpolation float3 texColor;
}

PSIn vsMain(GS3DMisc::PointVSIn vIn, uint instanceID: SV_InstanceID)
{
    var hitInfo = (GStaticScene::HitInfo)0;
    hitInfo.isHit = true;
    hitInfo.isFrontFace = true;
    hitInfo.instanceID = instanceID;
    hitInfo.meshPrimitiveID = vIn.primitiveID;
    hitInfo.barycentrics = vIn.barycentrics;

    var hitPoint = gGStaticScene.getHitPoint(hitInfo);

    PSIn o;
    o.clipPos = mul(gGStaticScene.camera.getViewProj(), float4(hitPoint.position, 1));
    o.texColor = gGStaticScene.textures[hitPoint.textureID].SampleLevel(gGStaticScene.sampler, hitPoint.texcoord, 0).rgb;
    return o;
}

float4 psMain(PSIn pIn)
{
    return float4(pIn.texColor, 1);
}

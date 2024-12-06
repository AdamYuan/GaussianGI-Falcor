#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Scene.GMesh;
import GVBuffer;

struct GSIn
{
    float2 texcoord;
    float3 worldPos;
    float4 clipPos;
    uint meshInstanceID;
    uint meshFirstIndex;
}

struct PSIn
{
    linear float2 texcoord;
    linear float3 worldPos;
    linear float4 clipPos : SV_Position;
    nointerpolation uint textureID;
    nointerpolation uint meshPrimitiveID;
    nointerpolation uint meshInstanceID;
}

GSIn vsMain(GMesh::VSIn vIn, uint instanceID: SV_InstanceID)
{
    var instanceInfo = gGStaticScene.getInstanceInfo(instanceID);
    var meshInfo = gGStaticScene.getMeshInfo(instanceInfo.meshID);

    GSIn o;
    o.texcoord = vIn.texcoord;
    o.worldPos = instanceInfo.transform.apply(vIn.position);
    o.clipPos = mul(gGStaticScene.camera.getViewProj(), float4(o.worldPos, 1));
    o.meshInstanceID = GVBuffer::encodeMeshInstanceID(instanceInfo.meshID, instanceID);
    o.meshFirstIndex = meshInfo.firstIndex;

    return o;
}

[maxvertexcount(3)]
void gsMain(triangle GSIn gIn[3], uint primitiveID: SV_PrimitiveID, inout TriangleStream<PSIn> output)
{
    PSIn o;
    o.meshInstanceID = gIn[0].meshInstanceID;
    o.meshPrimitiveID = primitiveID;
    uint globalPrimitiveID = GStaticScene::getGlobalPrimitiveID(gIn[0].meshFirstIndex, primitiveID);
    o.textureID = gGStaticScene.getPrimitiveTextureID(globalPrimitiveID);

    [ForceUnroll]
    for (int i = 0; i < 3; ++i)
    {
        o.texcoord = gIn[i].texcoord;
        o.worldPos = gIn[i].worldPos;
        o.clipPos = gIn[i].clipPos;
        output.Append(o);
    }
}

float4 psMain(PSIn pIn, bool isFrontFace: SV_IsFrontFace)
{
    float4 texColor = gGStaticScene.textures[pIn.textureID].Sample(gGStaticScene.sampler, pIn.texcoord);
    if (texColor.a < 0.5)
        discard;

    return float4(texColor.rgb, 1);
}

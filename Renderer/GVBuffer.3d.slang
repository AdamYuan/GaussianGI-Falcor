#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Scene.GMesh;
import GaussianGI.Algorithm.ShaderMisc;
import GVBuffer;

struct GSIn
{
    float2 texcoord;
    float3 worldPos;
    float4 clipPos;
    uint instanceID;
    uint meshFirstPrimitive;
}

struct PSIn
{
    linear float2 texcoord;
    linear float3 worldPos;
    linear float4 clipPos : SV_Position;
    nointerpolation uint textureID;
    nointerpolation uint meshPrimitiveID;
    nointerpolation uint instanceID;
}

GSIn vsMain(GMesh::VSIn vIn, uint instanceID: SV_InstanceID)
{
    var instanceInfo = gGStaticScene.getInstanceInfo(instanceID);
    var meshInfo = gGStaticScene.getMeshInfo(instanceInfo.meshID);

    GSIn o;
    o.texcoord = vIn.texcoord;
    o.worldPos = instanceInfo.transform.apply(vIn.position);
    o.clipPos = mul(gGStaticScene.camera.getViewProj(), float4(o.worldPos, 1));
    o.instanceID = instanceID;
    o.meshFirstPrimitive = meshInfo.firstIndex / 3;

    return o;
}

[maxvertexcount(3)]
void gsMain(triangle GSIn gIn[3], uint primitiveID: SV_PrimitiveID, inout TriangleStream<PSIn> output)
{
    PSIn o;
    o.instanceID = gIn[0].instanceID;
    o.meshPrimitiveID = primitiveID;
    uint globalPrimitiveID = gIn[0].meshFirstPrimitive + primitiveID;
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

struct PSOut
{
    float4 albedo : SV_Target0;
    uint4 primitive : SV_Target0;
};

PSOut psMain(PSIn pIn, bool isFrontFace: SV_IsFrontFace, float3 barycentrics: SV_Barycentrics)
{
    float4 texColor = gGStaticScene.textures[pIn.textureID].Sample(gGStaticScene.sampler, pIn.texcoord);
    if (alphaShouldDiscard(texColor.a))
        discard;

    PSOut o;
    o.albedo = float4(texColor.rgb, 1);
    uint backFaceFlag = select(isFrontFace, 0, 1 << 31);
    o.primitive = uint4(pIn.meshPrimitiveID | backFaceFlag, pIn.instanceID, asuint(barycentrics.yz));
    return o;
}

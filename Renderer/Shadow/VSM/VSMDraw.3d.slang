#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GStaticScene;
import GaussianGI.Scene.GMesh;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Algorithm.ShadowMapTransform;

uniform ShadowMapTransform gSMTransform;

struct GSIn
{
    float2 texcoord;
    float4 clipPos;
    uint meshFirstPrimitive;
}

struct PSIn
{
    linear float2 texcoord;
    linear float4 clipPos : SV_Position;
    nointerpolation uint textureID;
}

GSIn vsMain(GMesh::VSIn vIn, uint instanceID: SV_InstanceID)
{
    var instanceInfo = gGStaticScene.getInstanceInfo(instanceID);
    var meshInfo = gGStaticScene.getMeshInfo(instanceInfo.meshID);
    float3 worldPos = instanceInfo.transform.apply(vIn.position);

    GSIn o;
    o.texcoord = vIn.texcoord;
    o.clipPos = float4(gSMTransform.getClipPos(worldPos), 1);
    o.meshFirstPrimitive = meshInfo.firstIndex / 3;

    return o;
}

[maxvertexcount(3)]
void gsMain(triangle GSIn gIn[3], uint primitiveID: SV_PrimitiveID, inout TriangleStream<PSIn> output)
{
    PSIn o;
    uint globalPrimitiveID = gIn[0].meshFirstPrimitive + primitiveID;
    o.textureID = gGStaticScene.getPrimitiveTextureID(globalPrimitiveID);

    [ForceUnroll]
    for (int i = 0; i < 3; ++i)
    {
        o.texcoord = gIn[i].texcoord;
        o.clipPos = gIn[i].clipPos;
        output.Append(o);
    }
}

float2 psMain(PSIn pIn)
{
    float4 texColor = gGStaticScene.textures[pIn.textureID].Sample(gGStaticScene.sampler, pIn.texcoord);
    if (alphaShouldDiscard(texColor.a))
        discard;
    float depth = pIn.clipPos.z;
    return float2(depth, depth * depth);
}

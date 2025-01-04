#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GScene;
import GaussianGI.Scene.GMesh;
import GaussianGI.Util.ShaderUtil;

struct GSIn
{
    float2 texcoord;
    float3 worldPos;
    float3 normal;
    float4 clipPos;
}

struct PSIn
{
    linear float2 texcoord;
    linear float3 worldPos;
    linear float3 normal;
    linear float4 clipPos : SV_Position;
    nointerpolation uint textureID;
}

GSIn vsMain(GMesh::VSIn vIn)
{
    GSIn o;
    o.texcoord = vIn.texcoord;
    o.worldPos = gGScene.transform.apply(vIn.position);
    o.normal = gGScene.transform.applyRotation(vIn.normal);
    o.clipPos = mul(gGScene.camera.getViewProj(), float4(o.worldPos, 1));

    return o;
}

[maxvertexcount(3)]
void gsMain(triangle GSIn gIn[3], uint primitiveID: SV_PrimitiveID, inout TriangleStream<PSIn> output)
{
    PSIn o;
    o.textureID = gGScene.rasterData.getTextureID(primitiveID);

    [ForceUnroll]
    for (int i = 0; i < 3; ++i)
    {
        o.texcoord = gIn[i].texcoord;
        o.worldPos = gIn[i].worldPos;
        o.normal = gIn[i].normal;
        o.clipPos = gIn[i].clipPos;
        output.Append(o);
    }
}

float4 psMain(PSIn pIn, bool isFrontFace: SV_IsFrontFace)
{
    float3 normal = normalize(pIn.normal);
    normal = select(isFrontFace, normal, -normal);
    float4 texColor = gGScene.rasterData.textures[pIn.textureID].Sample(gGScene.sampler, pIn.texcoord);
    if (alphaShouldDiscard(texColor.a))
        discard;

    float3 brdf = texColor.rgb * M_1_PI;
    float3 radiance = brdf * gGScene.lighting.evaluateSunIrradiance(normal);

    return float4(radiance, 1);
}

#include "GaussianGI/Scene/GMesh.slangh"

import GaussianGI.Scene.GMesh;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Algorithm.MeshGSTrainer.MeshGSTrainer;

uniform MeshGSTrainer::Camera gCamera;
GMesh::RasterData gMeshRasterData;
SamplerState gSampler;

struct GSIn
{
    float viewZ;
    float2 texcoord;
    float4 clipPos;
}

struct PSIn
{
    linear float viewZ;
    linear float2 texcoord;
    linear float4 clipPos : SV_Position;
    nointerpolation uint textureID;
}

GSIn vsMain(GMesh::VSIn vIn)
{
    GSIn o;
    o.texcoord = vIn.texcoord;
    float3 viewPos = mul(gCamera.viewMat, float4(vIn.position, 1)).xyz;
    o.viewZ = viewPos.z;
    o.clipPos = mul(gCamera.projMat, float4(viewPos, 1));

    return o;
}

[maxvertexcount(3)]
void gsMain(triangle GSIn gIn[3], uint primitiveID: SV_PrimitiveID, inout TriangleStream<PSIn> output)
{
    PSIn o;
    o.textureID = gMeshRasterData.getTextureID(primitiveID);

    [ForceUnroll]
    for (int i = 0; i < 3; ++i)
    {
        o.viewZ = gIn[i].viewZ;
        o.texcoord = gIn[i].texcoord;
        o.clipPos = gIn[i].clipPos;
        output.Append(o);
    }
}

MeshGSTrainer::MeshRTOut psMain(PSIn pIn, bool isFrontFace: SV_IsFrontFace)
{
    float4 texColor = gMeshRasterData.textures[pIn.textureID].Sample(gSampler, pIn.texcoord);
    if (alphaShouldDiscard(texColor.a))
        discard;

    float depth = MeshGSTrainer::viewZ2Depth(pIn.viewZ, gCamera.nearZ, gCamera.farZ);
    return MeshGSTrainer::MeshRTOut::create(depth);
}

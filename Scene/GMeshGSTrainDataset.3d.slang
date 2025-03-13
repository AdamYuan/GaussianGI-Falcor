#include "GaussianGI/Scene/GMesh.slangh"

import GaussianGI.Scene.GMesh;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Algorithm.MeshGSTrainer.MeshGSTrainer;

#ifndef TRAIT
#define TRAIT DEPTH_TRAIT
#endif

#define DEPTH_TRAIT 0
#define DEPTH_ALBEDO_TRAIT 1
#define DEPTH_ALBEDO_NORMAL_TRAIT 2

uniform MeshGSTrain::Camera gCamera;
GMesh::RasterData gMeshRasterData;
SamplerState gSampler;

struct GSIn
{
    float viewZ;
    float2 texcoord;
    float3 position;
    float4 clipPos;
}

struct PSIn
{
    linear float viewZ;
    linear float2 texcoord;
    linear float4 clipPos : SV_Position;
    nointerpolation float3 geomNormal;
    nointerpolation uint textureID;
}

GSIn vsMain(GMesh::VSIn vIn)
{
    GSIn o;
    o.texcoord = vIn.texcoord;
    o.position = vIn.position;
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
    o.geomNormal = cross(gIn[1].position - gIn[0].position, gIn[2].position - gIn[0].position);
    o.geomNormal = normalize(o.geomNormal);

    [ForceUnroll]
    for (int i = 0; i < 3; ++i)
    {
        o.viewZ = gIn[i].viewZ;
        o.texcoord = gIn[i].texcoord;
        o.clipPos = gIn[i].clipPos;
        output.Append(o);
    }
    output.RestartStrip();
}

MeshGSTrain::Trait::MeshRTOut psMain(PSIn pIn, bool isFrontFace: SV_IsFrontFace)
{
    float4 texColor = gMeshRasterData.textures[pIn.textureID].Sample(gSampler, pIn.texcoord);
    if (alphaShouldDiscard(texColor.a))
        discard;

    float3 normal = pIn.geomNormal;
    normal = select(isFrontFace, normal, -normal);

#if TRAIT == DEPTH_TRAIT
    MeshGSTrain::Trait::SplatChannel channel;
    channel.depth = MeshGSTrain::Trait::viewZ2Depth(pIn.viewZ, gCamera.nearZ, gCamera.farZ);
    return MeshGSTrain::Trait::MeshRTOut::create(channel);
#elif TRAIT == DEPTH_ALBEDO_TRAIT
    MeshGSTrain::Trait::SplatChannel channel;
    channel.albedo = texColor.xyz;
    channel.depth = MeshGSTrain::Trait::viewZ2Depth(pIn.viewZ, gCamera.nearZ, gCamera.farZ);
    return MeshGSTrain::Trait::MeshRTOut::create(channel);
#elif TRAIT == DEPTH_ALBEDO_NORMAL_TRAIT
    MeshGSTrain::Trait::SplatChannel channel;
    channel.albedo = texColor.xyz;
    channel.depth = MeshGSTrain::Trait::viewZ2Depth(pIn.viewZ, gCamera.nearZ, gCamera.farZ);
    channel.normal = normal;
    return MeshGSTrain::Trait::MeshRTOut::create(channel);
#else
#error
#endif
}

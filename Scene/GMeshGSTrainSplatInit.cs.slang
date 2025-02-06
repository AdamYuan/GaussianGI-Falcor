import GMesh;
import GaussianGI.Algorithm.MeshGSTrainer.MeshGSTrainer;
import GaussianGI.Algorithm.ShaderMisc;
import Utils.Math.Quaternion;

struct MeshPoint
{
    uint primitiveID;
    float2 barycentrics;
}

GMesh::Data gGMeshData;
MeshGSTrain::RWSplatBuffer gSplatBuffer;
StructuredBuffer<MeshPoint> gMeshPoints;
uniform uint gSplatCount;
uniform float gSplatScale;
SamplerState gSampler;

#ifndef TRAIT
#define TRAIT DEPTH_TRAIT
#endif

#define DEPTH_TRAIT 0
#define DEPTH_ALBEDO_TRAIT 1

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint idx = threadID.x;
    if (idx >= gSplatCount)
        return;

    MeshPoint meshPoint = gMeshPoints[idx];

    GMesh::Vertex vertices[3];
    gGMeshData.getVertices(meshPoint.primitiveID, vertices);

    float3 baryWeights = float3(1 - meshPoint.barycentrics.x - meshPoint.barycentrics.y, meshPoint.barycentrics);
    float3 position = mul(baryWeights, float3x3(vertices[0].position, vertices[1].position, vertices[2].position));
    float2 texcoord = mul(baryWeights, float3x2(vertices[0].texcoord, vertices[1].texcoord, vertices[2].texcoord));
    float3 normal = normalize(cross(vertices[1].position - vertices[0].position, vertices[2].position - vertices[0].position));

    MeshGSTrain::Splat splat;

    splat.geom.mean = position;
    splat.geom.scale = float3(gSplatScale, gSplatScale, gSplatScale * 0.1);
    splat.geom.rotate = from_to_rotation(float3(0, 0, 1), normal);

    float4 texColor = gGMeshData.textures[gGMeshData.getTextureID(meshPoint.primitiveID)].SampleLevel(gSampler, texcoord, 0);
    if (alphaShouldDiscard(texColor.a))
        texColor = float4(0, 0, 0, 0);

#if TRAIT == DEPTH_TRAIT

#elif TRAIT == DEPTH_ALBEDO_TRAIT
    splat.attrib.albedo = texColor.rgb;
#else
#error
#endif

    gSplatBuffer.store(idx, splat);
}

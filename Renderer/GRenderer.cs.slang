#include "Utils/Math/MathConstants.slangh"
import GaussianGI.Common.ThreadGroupTilingX;
import GaussianGI.Scene.GStaticScene;
import GVBuffer;

ParameterBlock<GVBuffer> gGVBuffer;
RWTexture2D<unorm float4> gTarget;

#define GROUP_DIM 8

[numthreads(GROUP_DIM, GROUP_DIM, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint2 threadID =
        ThreadGroupTilingX((gGVBuffer.resolution + GROUP_DIM - 1) / GROUP_DIM, uint2(GROUP_DIM), 8, groupThreadID.xy, groupID.xy);

    if (any(threadID >= gGVBuffer.resolution))
        return;

    GVBuffer::Texel vTexel = gGVBuffer.Load(threadID);
    if (!vTexel.isHit)
    {
        gTarget[threadID] = float4(0, 0, 0, 1);
        return;
    }

    uint globalPrimitiveID = gGStaticScene.getGlobalPrimitiveID(vTexel.meshID, vTexel.meshPrimitiveID);
    GMesh::Vertex vertices[3];
    gGStaticScene.getPrimitiveVertices(globalPrimitiveID, vertices);

    float3 baryWeights = float3(vTexel.barycentrics, 1 - vTexel.barycentrics.x - vTexel.barycentrics.y);
    float3 normal = mul(baryWeights, float3x3(vertices[0].normal, vertices[1].normal, vertices[2].normal));
    normal = normalize(normal);
    normal = select(vTexel.isFrontFace, normal, -normal);

    float3 brdf = vTexel.albedo * M_1_PI;
    float3 radiance = brdf * gGStaticScene.lighting.evaluateIrradiance(normal);

    gTarget[threadID] = float4(radiance, 1);
}

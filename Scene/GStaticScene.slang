#include "GMesh.slangh"
__exported import GMesh;
__exported import GLighting;
__exported import GTransform;
__exported import Scene.Camera.Camera;

public struct GStaticScene
{
    public struct HitInfo
    {
        public bool isHit;
        public bool isFrontFace;
        public uint meshPrimitiveID;
        public uint meshID;
        public uint instanceID;
        public float2 barycentrics;
    };
    public struct HitPoint
    {
        public float3 position;
        public float3 normal;
        public float3 geomNormal;
        public uint textureID;
        public float2 texcoord;
    };
    public struct MeshInfo
    {
        public uint indexCount, instanceCount, firstIndex, firstInstance;
    };
    public struct InstanceInfo
    {
        public GTransform transform;
        public uint meshID;
    };
    StructuredBuffer<GMesh::Vertex> vertices;
    StructuredBuffer<uint> indices;
    Buffer<uint> textureIDs;
    StructuredBuffer<MeshInfo> meshInfos;
    StructuredBuffer<InstanceInfo> instanceInfos;
    public Texture2D<float4> textures[GMESH_MAX_TEXTURE_COUNT];
    public SamplerState sampler;
    public Camera camera;
    public GLighting lighting;

    public GMesh::Vertex getVertex(uint index) { return vertices[index]; }
    public MeshInfo getMeshInfo(uint meshID) { return meshInfos[meshID]; }
    public InstanceInfo getInstanceInfo(uint instanceID) { return instanceInfos[instanceID]; }

    public static uint getGlobalPrimitiveID(in const MeshInfo meshInfo, uint meshPrimitiveID)
    {
        return meshInfo.firstIndex / 3 + meshPrimitiveID;
    }
    public uint getGlobalPrimitiveID(uint meshID, uint meshPrimitiveID)
    {
        return getGlobalPrimitiveID(getMeshInfo(meshID), meshPrimitiveID);
    }
    /* public uint3 getPrimitiveIndices(uint globalPrimitiveID)
    {
        return uint3(indices[globalPrimitiveID * 3], indices[globalPrimitiveID * 3 + 1], indices[globalPrimitiveID * 3 + 2]);
    } */
    public void getPrimitiveVertices(uint globalPrimitiveID, out GMesh::Vertex o_primitiveVertices[3])
    {
        var primitiveIndices =
            uint3(indices[globalPrimitiveID * 3], indices[globalPrimitiveID * 3 + 1], indices[globalPrimitiveID * 3 + 2]);
        o_primitiveVertices[0] = getVertex(primitiveIndices[0]);
        o_primitiveVertices[1] = getVertex(primitiveIndices[1]);
        o_primitiveVertices[2] = getVertex(primitiveIndices[2]);
    }
    public uint getPrimitiveTextureID(uint globalPrimitiveID) { return textureIDs[globalPrimitiveID]; }

    public HitPoint getHitPoint(HitInfo hitInfo)
    {
        if (!hitInfo.isHit)
            return (HitPoint)0;

        uint globalPrimitiveID = getGlobalPrimitiveID(hitInfo.meshID, hitInfo.meshPrimitiveID);
        GMesh::Vertex vertices[3];
        getPrimitiveVertices(globalPrimitiveID, vertices);

        float3 baryWeights = float3(hitInfo.barycentrics, 1 - hitInfo.barycentrics.x - hitInfo.barycentrics.y);

        float3 position = mul(baryWeights, float3x3(vertices[0].position, vertices[1].position, vertices[2].position));

        float3 normal = mul(baryWeights, float3x3(vertices[0].normal, vertices[1].normal, vertices[2].normal));
        normal = normalize(normal);
        normal = select(hitInfo.isFrontFace, normal, -normal);

        float3 geomNormal = normalize(cross(vertices[1].position - vertices[0].position, vertices[2].position - vertices[0].position));
        geomNormal = select(hitInfo.isFrontFace, geomNormal, -geomNormal);

        uint textureID = getPrimitiveTextureID(globalPrimitiveID);

        float2 texcoord = mul(baryWeights, float3x2(vertices[0].texcoord, vertices[1].texcoord, vertices[2].texcoord));

        HitPoint hitPoint;
        hitPoint.position = position;
        hitPoint.normal = normal;
        hitPoint.geomNormal = geomNormal;
        hitPoint.textureID = textureID;
        hitPoint.texcoord = texcoord;

        return hitPoint;
    }
}

public ParameterBlock<GStaticScene> gGStaticScene;

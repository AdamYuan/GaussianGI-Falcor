#include "GMesh.slangh"
__exported import GMesh;
__exported import GLighting;
__exported import GTransform;
__exported import Scene.Camera.Camera;

public struct GStaticScene
{
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
        var primitiveIndices = uint3(indices[globalPrimitiveID * 3], indices[globalPrimitiveID * 3 + 1], indices[globalPrimitiveID * 3 + 2]);
        o_primitiveVertices[0] = getVertex(primitiveIndices[0]);
        o_primitiveVertices[1] = getVertex(primitiveIndices[1]);
        o_primitiveVertices[2] = getVertex(primitiveIndices[2]);
    }
    public uint getPrimitiveTextureID(uint globalPrimitiveID) { return textureIDs[globalPrimitiveID]; }
}

public ParameterBlock<GStaticScene> gGStaticScene;

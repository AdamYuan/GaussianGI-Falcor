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

    public static uint getGlobalPrimitiveID(uint meshFirstIndex, uint meshPrimitiveID) { return meshFirstIndex / 3 + meshPrimitiveID; }
    public static uint getGlobalPrimitiveID(in const MeshInfo meshInfo, uint meshPrimitiveID)
    {
        return getGlobalPrimitiveID(meshInfo.firstIndex, meshPrimitiveID);
    }
    public uint3 getPrimitiveIndices(uint globalPrimitiveID)
    {
        return uint3(indices[globalPrimitiveID * 3], indices[globalPrimitiveID * 3 + 1], indices[globalPrimitiveID * 3 + 2]);
    }
    public uint getPrimitiveTextureID(uint globalPrimitiveID) { return textureIDs[globalPrimitiveID]; }
}

public ParameterBlock<GStaticScene> gGStaticScene;

#include "GMesh.slangh"

public namespace GMesh
{
    public struct VSIn
    {
        public float3 position : GMESH_POSITION;
        public float3 normal : GMESH_NORMAL;
        public float2 texcoord : GMESH_TEXCOORD;
    };

    public struct Vertex
    {
        public float3 position;
        public float3 normal;
        public float2 texcoord;
    };

    public struct RasterData
    {
        public Buffer<uint> textureIDs;
        public Texture2D<float4> textures[GMESH_MAX_TEXTURE_COUNT];

        public uint getTextureID(uint primitiveID) { return textureIDs[primitiveID]; }
    };

    public struct Data
    {
        public StructuredBuffer<Vertex> vertices;
        public StructuredBuffer<uint> indices;
        public Buffer<uint> textureIDs;
        public Texture2D<float4> textures[GMESH_MAX_TEXTURE_COUNT];

        public void getVertices(uint primitiveID, out GMesh::Vertex o_primitiveVertices[3])
        {
            var primitiveIndices = uint3(indices[primitiveID * 3], indices[primitiveID * 3 + 1], indices[primitiveID * 3 + 2]);
            o_primitiveVertices[0] = vertices[primitiveIndices[0]];
            o_primitiveVertices[1] = vertices[primitiveIndices[1]];
            o_primitiveVertices[2] = vertices[primitiveIndices[2]];
        }

        public uint getTextureID(uint primitiveID) { return textureIDs[primitiveID]; }
    }
}

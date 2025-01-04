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
}

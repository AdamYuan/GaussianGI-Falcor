#include "GMesh.slangh"

__exported import GLighting;
__exported import GTransform;
__exported import Scene.Camera.Camera;

public struct GScene
{
    Buffer<uint> textureIDs;

    public Texture2D<float4> textures[GMESH_MAX_TEXTURE_COUNT];
    public Camera camera;
    public SamplerState sampler;
    public GLighting lighting;
    public GTransform transform;

    public uint getTextureID(uint primitiveID) { return textureIDs[primitiveID]; }
}

public ParameterBlock<GScene> gGScene;

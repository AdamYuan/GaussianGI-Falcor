#include "GMesh.slangh"

__exported import GLighting;
__exported import GTransform;
__exported import Scene.Camera.Camera;

public struct GScene
{
    Texture2D<float4> textures[GMESH_MAX_TEXTURE_COUNT];
    Buffer<uint> textureIDs;

    public Camera camera;
    public SamplerState sampler;
    public GLighting lighting;
    public GTransform transform;

    public Texture2D<float4> getTexture(uint primitiveID) { return textures[textureIDs[primitiveID]]; }
}

public ParameterBlock<GScene> gGScene;

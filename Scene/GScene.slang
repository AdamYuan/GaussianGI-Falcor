#include "GMesh.slangh"

__exported import GTransform;
__exported import Scene.Camera.Camera;

public struct GScene
{
    Texture2D<float4> textures[256];
    GTransform transforms[256];
    Buffer<uint> textureIDs;

    public Camera camera;
    public SamplerState sampler;

    public GTransform getTransform(uint instanceID) { return transforms[instanceID]; }
    public Texture2D<float4> getTexture(uint primitiveID) { return textures[textureIDs[primitiveID]]; }
}

public ParameterBlock<GScene> gGScene;

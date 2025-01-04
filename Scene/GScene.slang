#include "GMesh.slangh"

__exported import GMesh;
__exported import GLighting;
__exported import GTransform;
__exported import Scene.Camera.Camera;

public struct GScene
{
    public GMesh::RasterData rasterData;
    public Camera camera;
    public SamplerState sampler;
    public GLighting lighting;
    public GTransform transform;
}

public ParameterBlock<GScene> gGScene;

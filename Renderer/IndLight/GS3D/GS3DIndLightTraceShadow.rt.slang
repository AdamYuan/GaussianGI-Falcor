import GaussianGI.Scene.GStaticScene;
import GaussianGI.Renderer.IndLight.GS3D.GS3DIndLightSplat;
import GaussianGI.Algorithm.GS3D;
#include "GaussianGI/Renderer/IndLight/GS3D/GS3DAccelStructPrimitive.slangh"
#include "Utils/Math/MathConstants.slangh"

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
RaytracingAccelerationStructure gSplatAccel;
RWBuffer<float> gSplatShadows;

struct RayData
{
    float t;
}

[shader("anyhit")]
void anyHit(inout RayData rayData, BuiltInTriangleIntersectionAttributes attribs)
{
    uint meshSplatOfst = PrimitiveIndex() / AS_PRIM_TRIANGLE_COUNT;
    uint firstMeshSplatID = InstanceID();
    uint instanceID = InstanceIndex();

    var splatGeom = gSplats.load(firstMeshSplatID + meshSplatOfst, instanceID).geom;

    float2x3 splatCov3D = GS3D::getCovariance3D(splatGeom.rotate, splatGeom.scale);
    float alpha = GS3D::getRayAlpha(WorldRayOrigin(), WorldRayDirection(), splatCov3D, splatGeom.mean);

    rayData.t *= (1 - alpha);

    if (rayData.t > 1.0 / 255.0)
        IgnoreHit();
}

[shader("closesthit")]
void closestHit(inout RayData rayData, BuiltInTriangleIntersectionAttributes attribs) {}

[shader("miss")]
void miss(inout RayData rayData) {}

[shader("raygeneration")]
void rayGen()
{
    uint splatID = DispatchRaysIndex().x;
    var splatGeom = gSplats.load(splatID).geom;

    Ray ray;
    ray.origin = splatGeom.mean;
    ray.dir = gGStaticScene.lighting.direction;
    ray.tMin = 0;
    ray.tMax = FLT_MAX;

    RayData rayData;
    rayData.t = 1.0;

    TraceRay(gSplatAccel, AS_PRIM_CULL_FLAG | RAY_FLAG_FORCE_NON_OPAQUE, 0xFF, 0, 1, 0, ray.toRayDesc(), rayData);

    gSplatShadows[splatID] = rayData.t;
}


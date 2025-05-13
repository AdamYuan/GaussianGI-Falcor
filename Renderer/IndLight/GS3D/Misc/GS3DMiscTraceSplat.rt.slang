import GaussianGI.Scene.GStaticScene;
import Utils.Sampling.TinyUniformSampleGenerator;
import GaussianGI.Renderer.IndLight.GS3D.GS3DIndLightSplat;
import GaussianGI.Algorithm.SphericalHarmonics;
import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.ShaderMisc;
import GS3DMiscColor;
#include "GaussianGI/Renderer/IndLight/GS3D/GS3DAccelStructPrimitive.slangh"

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
RaytracingAccelerationStructure gSplatAccel;
RWTexture2D<unorm float4> gColor;

Buffer<float> gSplatShadows;
StructuredBuffer<SH9<float16_t3>> gSplatProbes;

struct RayData
{
    TinyUniformSampleGenerator randGen;
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

    float rand = sampleNext1D(rayData.randGen);
    if (rand >= alpha)
        IgnoreHit();
}

[shader("closesthit")]
void closestHit(inout RayData rayData, BuiltInTriangleIntersectionAttributes attribs)
{
    uint meshSplatOfst = PrimitiveIndex() / AS_PRIM_TRIANGLE_COUNT;
    uint firstMeshSplatID = InstanceID();
    uint instanceID = InstanceIndex();
    var splat = gSplats.load(firstMeshSplatID + meshSplatOfst, instanceID);
    uint splatID = gSplats.loadInstanceDesc(instanceID).firstSplatIdx + meshSplatOfst;
    uint2 coord = DispatchRaysIndex().xy;
    gColor[coord] = float4(getGS3DMiscColor(splat, gSplatShadows[splatID], gSplatProbes[splatID]), 1);
}

[shader("miss")]
void miss(inout RayData rayData)
{
    uint2 coord = DispatchRaysIndex().xy;
    gColor[coord] = float4(0, 0, 0, 1);
}

[shader("raygeneration")]
void rayGen()
{
    uint2 coord = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;

    Ray ray = gGStaticScene.camera.computeRayPinhole(coord, resolution, false);
    RayData rayData;
    rayData.randGen = TinyUniformSampleGenerator(coord, 0);
    TraceRay(gSplatAccel, AS_PRIM_CULL_FLAG | RAY_FLAG_FORCE_NON_OPAQUE, 0xFF, 0, 1, 0, ray.toRayDesc(), rayData);
}


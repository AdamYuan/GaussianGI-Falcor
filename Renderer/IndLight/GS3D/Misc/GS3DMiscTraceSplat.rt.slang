import GaussianGI.Scene.GStaticScene;
import Utils.Sampling.TinyUniformSampleGenerator;
import GaussianGI.Renderer.IndLight.GS3D.GS3DIndLightSplat;
import Utils.Math.Quaternion;
import GaussianGI.Algorithm.SphericalHarmonics;
import GS3DMiscColor;
#include "GaussianGI/Algorithm/Icosahedron.slangh"

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
RaytracingAccelerationStructure gSplatAccel;
RWTexture2D<unorm float4> gColor;

Buffer<float> gSplatShadows;
StructuredBuffer<SH<float3, 2>> gSplatProbes;

struct RayData
{
    TinyUniformSampleGenerator randGen;
}

[shader("anyhit")]
void anyHit(inout RayData rayData, BuiltInTriangleIntersectionAttributes attribs)
{
    uint meshSplatOfst = PrimitiveIndex() / ICOSAHEDRON_TRIANGLE_COUNT;
    uint firstMeshSplatID = InstanceID();
    uint instanceID = InstanceIndex();

    var splatGeom = gSplats.load(firstMeshSplatID + meshSplatOfst, instanceID).geom;

    float3 splatInvScale = 1.0 / splatGeom.scale;

    float3x3 splatRotMatT = float3x3(
        rotate_vector(float3(1, 0, 0), splatGeom.rotate), //
        rotate_vector(float3(0, 1, 0), splatGeom.rotate),
        float3(0, 0, 0)
    );
    splatRotMatT[2] = cross(splatRotMatT[0], splatRotMatT[1]);

    Ray ray;
    ray.origin = WorldRayOrigin();
    ray.dir = WorldRayDirection();
    float3 o_g = mul(splatRotMatT, ray.origin - splatGeom.mean) * splatInvScale;
    float3 d_g = mul(splatRotMatT, ray.dir) * splatInvScale;
    float t_max = -dot(o_g, d_g) / dot(d_g, d_g); // 3D Gaussian Ray Tracing, 4.3 Evaluating Particle Response
    if (isinf(t_max) || isnan(t_max))
        IgnoreHit();

    float3 x = ray.eval(t_max);
    float3 x_g = mul(splatRotMatT, x - splatGeom.mean) * splatInvScale;
    float power = max(dot(x_g, x_g), 0);
    float alpha = exp(-0.5 * power);

    float rand = sampleNext1D(rayData.randGen);
    if (rand >= alpha)
        IgnoreHit();
}

[shader("closesthit")]
void closestHit(inout RayData rayData, BuiltInTriangleIntersectionAttributes attribs)
{
    uint meshSplatOfst = PrimitiveIndex() / ICOSAHEDRON_TRIANGLE_COUNT;
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
    TraceRay(gSplatAccel, RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_FORCE_NON_OPAQUE, 0xFF, 0, 1, 0, ray.toRayDesc(), rayData);
}


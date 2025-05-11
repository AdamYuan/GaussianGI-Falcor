import GaussianGI.Scene.GStaticScene;
import Utils.Math.FormatConversion;
import GaussianGI.Renderer.IndLight.GS3D.GS3DIndLightSplat;
import GaussianGI.Algorithm.SphericalHarmonics;
import GS3DMiscColor;

using namespace GS3DIndLight;

#include "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightSplatPrimitive.slangh"

uniform InstancedSplatBuffer gSplats;
StructuredBuffer<uint> gSplatViewSortPayloads;
uniform float2 gResolution;

Buffer<float> gSplatShadows;
StructuredBuffer<SH<float3, 2>> gSplatProbes;

struct PSIn
{
    nointerpolation float3 color;
    linear float2 quadPos;
    linear float4 clipPos : SV_Position;
#if GS_PRIMITIVE_IS_RAY
    nointerpolation float2x3 splatCov3D;
    nointerpolation float3 splatMean;
#endif
}

uint vsMain(uint instanceID: SV_InstanceID)
{
    return instanceID;
}

[maxvertexcount(4)]
void gsMain(point uint gIn[1], inout TriangleStream<PSIn> output)
{
    uint splatID = gSplatViewSortPayloads[gIn[0]];
    var splat = gSplats.load(splatID);

    GSPrimitive gsPrim = GSPrimitive::create(splat.geom, gGStaticScene.camera, gResolution);

    PSIn o;
    o.color = getGS3DMiscColor(splat, gSplatShadows[splatID], gSplatProbes[splatID]);
#if GS_PRIMITIVE_IS_RAY
    o.splatCov3D = gsPrim.splatCov3D;
    o.splatMean = splat.geom.mean;
#endif

    [ForceUnroll]
    for (uint i = 0; i < 4; ++i)
    {
        o.quadPos = gsPrim.getVertexQuadPos(i);
        o.clipPos = gsPrim.getVertexClipPos(i);
        output.Append(o);
    }
    output.RestartStrip();
}

float4 psMain(PSIn pIn)
{
#if GS_PRIMITIVE_IS_RAY
    float alpha = GSPrimitive::getAlpha(gGStaticScene.camera, uint2(gResolution), uint2(pIn.clipPos.xy), pIn.splatCov3D, pIn.splatMean);
#else
    float alpha = GSPrimitive::getAlpha(pIn.quadPos);
#endif
    if (alpha < 1.0 / 255.0)
        discard;
    return float4(pIn.color, alpha);
}


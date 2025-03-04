#include "Utils/Math/MathConstants.slangh"
import Utils.Math.Quaternion;
import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Algorithm.SphericalHarmonics;
import GaussianGI.Algorithm.SGGX;
import GaussianGI.Renderer.IndLight.GS3D.GS3DIndLightSplat;

using namespace GS3DIndLight;

#ifndef COLOR_VAR_NAME
#define COLOR_VAR_NAME albedo
#endif

public float3 getGS3DMiscColor(Splat splat, float shadow, SH<float3, 2> probe)
{
    float3x3 splatRotMatT = float3x3(
        rotate_vector(float3(1, 0, 0), splat.geom.rotate),
        rotate_vector(float3(0, 1, 0), splat.geom.rotate),
        rotate_vector(float3(0, 0, 1), splat.geom.rotate)
    );
    float3x3 splatRotMat = transpose(splatRotMatT);
    float3 normal = SGGX::getSplatNormal(splatRotMat, splat.geom.scale, normalize(gGStaticScene.camera.getPosition() - splat.geom.mean));
    float3 albedo = splat.attrib.albedo;
    float3 indirectLight = M_1_PI * shDiffuse(probe, shBasis<2>(normal));
    float3 directLight = M_1_PI * gGStaticScene.lighting.evaluateSunIrradiance(normal, shadow);
    float3 directRadiance = albedo * directLight;
    float3 radiance = albedo * (directLight + indirectLight);

    normal = normal * 0.5 + 0.5;
    return float3(COLOR_VAR_NAME);
}

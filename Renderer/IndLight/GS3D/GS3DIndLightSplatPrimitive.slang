import GS3DIndLightSplat;
import Scene.Camera.Camera;
import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.FrustumCull;
#include "GaussianGI/Algorithm/GS3DBound.slangh"

#define BOUND_FACTOR GS3D_BOUND_SQRT_2LOG_255

public namespace GS3DIndLight
{
    public struct NaiveGSPrimitive
    {
        float2 centerClip, deltaClip0, deltaClip1;
        bool depthCulled;

        public static NaiveGSPrimitive create(SplatGeom splatGeom, Camera camera, float2 resolution)
        {
            NaiveGSPrimitive prim;
            float3 viewPos = mul(camera.data.viewMat, float4(splatGeom.mean, 1)).xyz;
            float4 clipPos = mul(camera.data.projMat, float4(viewPos, 1));

            prim.depthCulled = clipPos.w <= 0;
            clipPos /= clipPos.w;

            prim.depthCulled |= clipPos.z <= 0 || clipPos.z > 1;
            prim.depthCulled |= any(abs(clipPos.xy) > 2.0);

            float2x3 cov3d = GS3D::getCovariance3D(splatGeom.rotate, float3(splatGeom.scale));
            float3 cov2d = GS3D::getCovariance2D(cov3d, viewPos, float3x3(camera.data.viewMat), camera.data.projMat[0][0], resolution);
            GS3D::Eigen<2> eigen2d = GS3D::decomposeCovariance2D(cov2d);

            float2 axis0 = eigen2d.eigen[0] * sqrt(eigen2d.lambda[0]);
            float2 axis1 = eigen2d.eigen[1] * sqrt(eigen2d.lambda[1]);
            float2 deltaClip0 = BOUND_FACTOR * axis0 * 2 / resolution;
            float2 deltaClip1 = BOUND_FACTOR * axis1 * 2 / resolution;

            prim.centerClip = clipPos.xy;
            prim.deltaClip0 = deltaClip0;
            prim.deltaClip1 = deltaClip1;

            return prim;
        }
        public bool isFrustumCulled() { return depthCulled || cullQuadClip(centerClip, deltaClip0, deltaClip1); }
        public float2 getVertexQuadPos(uint idx)
        {
            return float2(select(bool(idx & 1), -BOUND_FACTOR, BOUND_FACTOR), select(bool(idx >> 1), -BOUND_FACTOR, BOUND_FACTOR));
        }
        public float4 getVertexClipPos(uint idx)
        {
            return float4(
                centerClip + select(bool(idx & 1), -deltaClip0, deltaClip0) + select(bool(idx >> 1), -deltaClip1, deltaClip1), 0, 1
            );
        }
        public static float getAlpha(float2 quadPos) { return exp(-0.5 * dot(quadPos, quadPos)); }
    };

    public struct GSPPPrimitive
    {
        float4 centerClip, deltaClip0, deltaClip1;

        public static GSPPPrimitive create(SplatGeom splatGeom, Camera camera, float2 resolution)
        {
            float3x3 splatRotMat = GS3D::getQuatMatrix(splatGeom.rotate);
            float3x3 splatRotMatT = transpose(splatRotMat);

            float2x3 splatCov3D = GS3D::getCovariance3D(splatRotMat, splatGeom.scale);

            float3 splatCamMean = splatGeom.mean - camera.getPosition();
            float splatCamDist = length(splatCamMean);
            float3 splatViewDir = splatCamMean / splatCamDist;
            float3 splatViewSideDir = select(
                all(splatGeom.scale.x > splatGeom.scale.yz), //
                splatRotMatT[0],
                select(splatGeom.scale.y > splatGeom.scale.z, splatRotMatT[1], splatRotMatT[2])
            );
            splatViewSideDir = normalize(cross(splatViewDir, splatViewSideDir));

            float3x3 splatVDMatT = float3x3(splatViewSideDir, cross(splatViewDir, splatViewSideDir), splatViewDir);
            float3x3 splatVDMat = transpose(splatVDMatT);

            float3 splatCov2D = GS3D::getGSPPCovariance2D(splatCov3D, splatCamDist, splatVDMatT, 1.0);
            GS3D::Eigen<2> eigen2D = GS3D::decomposeCovariance2D(splatCov2D);

            float4 centerClip = mul(camera.data.projMatNoJitter, float4(mul(float3x3(camera.data.viewMat), splatViewDir), 1.0));
            float2 deltaQuad0 = eigen2D.eigen[0] * sqrt(eigen2D.lambda[0]) * BOUND_FACTOR;
            float2 deltaQuad1 = eigen2D.eigen[1] * sqrt(eigen2D.lambda[1]) * BOUND_FACTOR;
            float4 deltaClip0 =
                mul(camera.data.projMatNoJitter,
                    float4(mul(float3x3(camera.data.viewMat), splatVDMatT[0] * deltaQuad0.x + splatVDMatT[1] * deltaQuad0.y), 0.0));
            float4 deltaClip1 =
                mul(camera.data.projMatNoJitter,
                    float4(mul(float3x3(camera.data.viewMat), splatVDMatT[0] * deltaQuad1.x + splatVDMatT[1] * deltaQuad1.y), 0.0));

            GSPPPrimitive prim;
            prim.centerClip = centerClip;
            prim.deltaClip0 = deltaClip0;
            prim.deltaClip1 = deltaClip1;

            return prim;
        }
        public bool isFrustumCulled() { return cullQuadClip(centerClip, deltaClip0, deltaClip1); }
        public float2 getVertexQuadPos(uint idx)
        {
            return float2(select(bool(idx & 1), -BOUND_FACTOR, BOUND_FACTOR), select(bool(idx >> 1), -BOUND_FACTOR, BOUND_FACTOR));
        }
        public float4 getVertexClipPos(uint idx)
        {
            return centerClip + select(bool(idx & 1), -deltaClip0, deltaClip0) + select(bool(idx >> 1), -deltaClip1, deltaClip1);
        }
        public static float getAlpha(float2 quadPos) { return exp(-0.5 * dot(quadPos, quadPos)); }
    };

    public struct GSRayPrimitive
    {
        public float2x3 splatCov3D;
        float4 centerClip, deltaClip0, deltaClip1;

        public static GSRayPrimitive create(SplatGeom splatGeom, Camera camera, float2 resolution)
        {
            float3x3 splatRotMat = GS3D::getQuatMatrix(splatGeom.rotate);
            float3x3 splatRotMatT = transpose(splatRotMat);

            float2x3 splatCov3D = GS3D::getCovariance3D(splatRotMat, splatGeom.scale);

            float3 splatCamMean = splatGeom.mean - camera.getPosition();
            float splatCamDist = length(splatCamMean);
            float3 splatViewDir = splatCamMean / splatCamDist;
            float3 splatViewSideDir = select(
                all(splatGeom.scale.x > splatGeom.scale.yz), //
                splatRotMatT[0],
                select(splatGeom.scale.y > splatGeom.scale.z, splatRotMatT[1], splatRotMatT[2])
            );
            splatViewSideDir = normalize(cross(splatViewDir, splatViewSideDir));

            float3x3 splatVDMatT = float3x3(splatViewSideDir, cross(splatViewDir, splatViewSideDir), splatViewDir);
            float3x3 splatVDMat = transpose(splatVDMatT);

            float3 splatCov2D = GS3D::getGSPPCovariance2D(splatCov3D, splatCamDist, splatVDMatT, 1.0);
            GS3D::Eigen<2> eigen2D = GS3D::decomposeCovariance2D(splatCov2D);

            float3x3 splatScaleMat = GS3D::getScaleMatrix(splatGeom.scale);

            float3 splatVDMean = float3(0, 0, splatCamDist);
            float3x3 splatVDRotScaleMat = mul(splatVDMatT, mul(splatRotMat, splatScaleMat));

            float3 splatVDExtent = float3(length(splatVDRotScaleMat[0]), length(splatVDRotScaleMat[1]), length(splatVDRotScaleMat[2]));

            float3x3 splatExtent =
                float3x3(splatVDMatT[0] * splatVDExtent.x, splatVDMatT[1] * splatVDExtent.y, splatVDMatT[2] * splatVDExtent.z);
            splatExtent *= BOUND_FACTOR;

            float3 splatQuadCenter = splatGeom.mean - splatExtent[2];
            float4 splatClipQuadCenter = mul(camera.getViewProj(), float4(splatQuadCenter, 1));
            float2x4 splatClipExtentXY =
                float2x4(mul(camera.getViewProj(), float4(splatExtent[0], 0)), mul(camera.getViewProj(), float4(splatExtent[1], 0)));

            GSRayPrimitive prim;
            prim.splatCov3D = splatCov3D;
            prim.centerClip = splatClipQuadCenter;
            prim.deltaClip0 = splatClipExtentXY[0];
            prim.deltaClip1 = splatClipExtentXY[1];

            return prim;
        }
        public bool isFrustumCulled() { return cullQuadClip(centerClip, deltaClip0, deltaClip1); }
        public float2 getVertexQuadPos(uint idx) { return float2(0); }
        public float4 getVertexClipPos(uint idx)
        {
            return centerClip + select(bool(idx & 1), -deltaClip0, deltaClip0) + select(bool(idx >> 1), -deltaClip1, deltaClip1);
        }
        public static float getAlpha(Camera camera, uint2 resolution, uint2 fragCoord, float2x3 splatCov3D, float3 splatMean)
        {
            Ray ray = camera.computeRayPinhole(fragCoord, resolution, false);
            return GS3D::getRayAlpha(ray.origin, ray.dir, splatCov3D, splatMean);
        }
    };
}

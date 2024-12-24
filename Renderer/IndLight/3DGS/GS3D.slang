import Utils.Math.Quaternion;
import GaussianGI.Scene.GTransform;

public namespace GS3D
{
    public struct Splat
    {
        public float2 barycentrics;
        public uint primitiveID;
        public float16_t4 rotate;
        public float16_t2 scaleXY;
    }

    public struct SplatView
    {
        public float16_t2 axis0, axis1;
        public float16_t2 clipXY;
        public uint color; // temporally use RGBA8, should use HDR formats later
    }

    public float3x3 getQuatMatrix(float4 quat)
    {
        float x = quat.x;
        float y = quat.y;
        float z = quat.z;
        float w = quat.w;
        return float3x3(
            float3(1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)),
            float3(2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)),
            float3(2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y))
        );
    }

    public float2x3 getCovariance3D(float4 splatRotate, float2 splatScaleXY, in const GTransform transform)
    {
        float3 scale = float3(splatScaleXY, 0.1 * splatScaleXY.x) * transform.scale;
        float3x3 scaleMat = float3x3(scale.x, 0, 0, 0, scale.y, 0, 0, 0, scale.z);
        float3x3 transformRotMat = transform.getRotateMatrix();
        float3x3 splatRotMat = getQuatMatrix(splatRotate);
        float3x3 rotMat = mul(transformRotMat, splatRotMat);
        float3x3 rotScaleMat = mul(rotMat, scaleMat);
        float3x3 sigma = mul(rotScaleMat, transpose(rotScaleMat));
        return float2x3(float3(sigma._m00, sigma._m01, sigma._m02), float3(sigma._m11, sigma._m12, sigma._m22));
    }

    public float3 getCovariance2D(float2x3 cov3d, float3 viewPos, float3x3 viewRotMatrix, float4x4 projMatrix, float2 resolution)
    {
        float focal = resolution.x * projMatrix._m00 * 0.5;
        float3x3 J = float3x3(
            float3(focal / viewPos.z, 0, -(focal * viewPos.x) / (viewPos.z * viewPos.z)),
            float3(0, focal / viewPos.z, -(focal * viewPos.y) / (viewPos.z * viewPos.z)),
            float3(0, 0, 0)
        );
        float3x3 W = viewRotMatrix;
        float3x3 T = mul(J, W);
        float3x3 V = float3x3(
            float3(cov3d[0].x, cov3d[0].y, cov3d[0].z),
            float3(cov3d[0].y, cov3d[1].x, cov3d[1].y),
            float3(cov3d[0].z, cov3d[1].y, cov3d[1].z)
        );
        float3x3 cov = mul(T, mul(V, transpose(T)));
        return float3(cov._m00, cov._m01, cov._m11);
    }

    public struct Eigen<let N : int>
    {
        public vector<float, N> lambda;
        public matrix<float, N, N> eigen;
    };

    public Eigen<2> decomposeCovariance2D(float3 cov2d)
    {
        // same as in antimatter15/splat
        float diag1 = cov2d.x, diag2 = cov2d.z, offDiag = cov2d.y;
        float mid = 0.5f * (diag1 + diag2);
        float radius = length(float2((diag1 - diag2) / 2.0, offDiag));
        float lambda1 = mid + radius;
        float lambda2 = max(mid - radius, 0.1);
        float2 diagVec = normalize(float2(offDiag, lambda1 - diag1));
        diagVec.y = -diagVec.y;
        Eigen<2> eigen;
        eigen.lambda = float2(lambda1, lambda2);
        eigen.eigen = float2x2(diagVec, float2(diagVec.y, -diagVec.x));
        return eigen;
    }
}

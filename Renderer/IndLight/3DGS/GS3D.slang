import Utils.Math.Quaternion;
import GaussianGI.Scene.GTransform;

public namespace GS3D
{
    public struct PackedSplat
    {
        public float3 position;
        public float16_t4 rotate;
        public float16_t2 scaleXY;
    }

    public struct PackedSplatView
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

    public float2x3 getCovariance3D(float4 splatRotate, float3 splatScale, in const GTransform transform)
    {
        float3 scale = splatScale * transform.scale;
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
        // focal = 0.5 * h / tan(fov * 0.5),
        // projMatrix._m00 = h / (w * tan(fov * 0.5))
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
        // [a, b]
        // [b, c]
        float a = cov2d.x, b = cov2d.y, c = cov2d.z;
        // | a - \lambda, b           | = 0
        // | b          , c - \lambda |
        // \Rightarrow \lambda^2 - (a + c)\lambda + ac - b^2 = 0

        // \frac{a + c}{2}
        float mid = 0.5 * (a + c);
        // \frac{\sqrt{(a + c)^2 - 4 (ac - b^2)}}{2} = \frac{\sqrt{(a - c)^2 + (2b)^2}}{2}
        float radius = length(float2((a - c) * 0.5, b));

        float lambda1 = mid + radius;
        float lambda2 = mid - radius;

        // [ a - \lambda, b           ][ e_x ] = 0
        // [ b          , c - \lambda ][ e_y ]
        float2 eigen1 = normalize(float2(b, lambda1 - a));
        float2 eigen2 = float2(eigen1.y, -eigen1.x); // normalize(float2(b, lambda2 - a));
        Eigen<2> eigen;
        eigen.lambda = float2(lambda1, lambda2);
        eigen.eigen = float2x2(eigen1, eigen2);
        return eigen;
    }
}

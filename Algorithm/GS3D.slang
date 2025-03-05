import GaussianGI.Algorithm.ShaderMisc;

public namespace GS3D
{
    [Differentiable]
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

    [Differentiable]
    public float3x3 getScaleMatrix(float3 scale)
    {
        return float3x3(scale.x, 0, 0, 0, scale.y, 0, 0, 0, scale.z);
    }

    [Differentiable]
    public float getRayAlpha(float3 rayOrigin, float3 rayDir, float2x3 splatCov3D, float3 splatMean)
    {
        return getRayAlpha(rayOrigin, getAlignRotMatT(rayDir), splatCov3D, splatMean);
    }

    [Differentiable]
    public float getRayAlpha(float3 rayOrigin, float3x3 rayDirMatT, float2x3 splatCov3D, float3 splatMean)
    {
        float3x3 sigma = float3x3(splatCov3D[0], float3(splatCov3D[0].y, splatCov3D[1].xy), float3(splatCov3D[0].z, splatCov3D[1].yz));
        float2x2 raySigma = float2x2(mul(rayDirMatT, mul(sigma, transpose(rayDirMatT))));
        float3 rayCov2D = float3(raySigma[0], raySigma[1][1]);
        float3 rayConic = getConic(rayCov2D);
        float2 rayPos = mul(rayDirMatT, rayOrigin - splatMean).xy;

        float power = 0.5 * (rayConic.x * rayPos.x * rayPos.x + rayConic.z * rayPos.y * rayPos.y) //
                      + rayConic.y * rayPos.x * rayPos.y;
        return select(isinf(power) || isnan(power), 0, exp(-max(power, 0)));

        /*float3 splatInvScale = 1.0 / splatGeom.scale;

        float3x3 splatRotMatT = transpose(GS3D::getQuatMatrix(splatGeom.rotate));
        Ray ray;
        ray.origin = WorldRayOrigin();
        ray.dir = WorldRayDirection();
        float3 o_g = mul(splatRotMatT, ray.origin - splatGeom.mean) * splatInvScale;
        float3 d_g = mul(splatRotMatT, ray.dir) * splatInvScale;
        float t_max = -dot(o_g, d_g) / dot(d_g, d_g); // 3D Gaussian Ray Tracing, 4.3 Evaluating Particle Response
        if (isinf(t_max) || isnan(t_max))
            return 0;

        float3 x = ray.eval(t_max);
        float3 x_g = mul(splatRotMatT, x - splatGeom.mean) * splatInvScale;
        float power = max(dot(x_g, x_g), 0);
        float alpha = exp(-0.5 * power);
        return alpha; */
    }

    [Differentiable]
    public float2x3 getCovariance3D(float4 splatRotate, float3 splatScale)
    {
        return getCovariance3D(getQuatMatrix(splatRotate), splatScale);
    }

    [Differentiable]
    public float2x3 getCovariance3D(float3x3 splatRotMat, float3 splatScale)
    {
        float3x3 scaleMat = getScaleMatrix(splatScale);
        float3x3 rotScaleMat = mul(splatRotMat, scaleMat);
        float3x3 sigma = mul(rotScaleMat, transpose(rotScaleMat));
        return float2x3(float3(sigma[0][0], sigma[0][1], sigma[0][2]), float3(sigma[1][1], sigma[1][2], sigma[2][2]));
    }

    [Differentiable]
    public float2 project(float3 viewPos, no_diff float camProjMat00, no_diff float2 screenResolution)
    {
        // focal = 0.5 * h / tan(fov * 0.5),
        // projMatrix[0][0] = h / (w * tan(fov * 0.5))
        float focal = screenResolution.x * camProjMat00 * 0.5;
        return focal * viewPos.xy / viewPos.z;
    }

    [Differentiable]
    public float3 getCovariance2D(
        float2x3 cov3d,
        float3 viewPos,
        no_diff float3x3 camViewRotMat,
        no_diff float camProjMat00,
        no_diff float2 screenResolution
    )
    {
        // focal = 0.5 * h / tan(fov * 0.5),
        // projMatrix[0][0] = h / (w * tan(fov * 0.5))
        float focal = screenResolution.x * camProjMat00 * 0.5;
        float3x3 J = float3x3(
            float3(focal / viewPos.z, 0, -(focal * viewPos.x) / (viewPos.z * viewPos.z)),
            float3(0, focal / viewPos.z, -(focal * viewPos.y) / (viewPos.z * viewPos.z)),
            float3(0, 0, 0)
        );
        float3x3 W = camViewRotMat;
        float3x3 T = mul(J, W);
        float3x3 V = float3x3(
            float3(cov3d[0].x, cov3d[0].y, cov3d[0].z),
            float3(cov3d[0].y, cov3d[1].x, cov3d[1].y),
            float3(cov3d[0].z, cov3d[1].y, cov3d[1].z)
        );
        float3x3 cov = mul(T, mul(V, transpose(T)));
        return float3(cov[0][0], cov[0][1], cov[1][1]);
    }

    // GS++
    [Differentiable]
    public float3 getGSPPCovariance2D(
        float2x3 cov3d,
        float3 viewPos,
        no_diff float3x3 camViewRotMat,
        no_diff float camProjMat00,
        no_diff float2 screenResolution
    )
    {
        // focal = 0.5 * h / tan(fov * 0.5),
        // projMatrix[0][0] = h / (w * tan(fov * 0.5))
        float focal = screenResolution.x * camProjMat00 * 0.5;
        float viewPosLen = length(viewPos);
        float3x3 J = float3x3(float3(focal / viewPosLen, 0, 0), float3(0, focal / viewPosLen, 0), float3(0, 0, 0));
        float3x3 W = mul(getAlignRotMatT(viewPos / viewPosLen), camViewRotMat);
        float3x3 T = mul(J, W);
        float3x3 V = float3x3(
            float3(cov3d[0].x, cov3d[0].y, cov3d[0].z),
            float3(cov3d[0].y, cov3d[1].x, cov3d[1].y),
            float3(cov3d[0].z, cov3d[1].y, cov3d[1].z)
        );
        float3x3 cov = mul(T, mul(V, transpose(T)));
        return float3(cov[0][0], cov[0][1], cov[1][1]);
    }

    [Differentiable]
    public float3 getConic(float3 cov2d)
    {
        // Inverse matrix of cov2d
        float det = cov2d.x * cov2d.z - cov2d.y * cov2d.y;
        return float3(cov2d.z, -cov2d.y, cov2d.x) / det;
    }

    public struct Eigen<let N : int> : IDifferentiable
    {
        public vector<float, N> lambda;
        public matrix<float, N, N> eigen;
    };

    [Differentiable]
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

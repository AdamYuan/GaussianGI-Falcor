import GaussianGI.Algorithm.GS3D;

namespace MeshGSTrainer
{
    public const static float kOpacity = 0.975;                   // O
    public const static float kAlphaThreshold = 1.0 / 255.0;      // \alpha_T
    public const static float kQuadPosBound = 2.3486050619834185; // Q
    // \alpha = O \exp{-Q^2} = \alpha_T
    // \Rightarrow Q = \sqrt{\log{\frac{O}{\alpha_T}}}

    public struct Camera
    {
        public float2 resolution; // TODO: Replace with aspectRatio
        public float4x4 viewMat, projMat;
        public float nearZ, farZ;
    };

    public struct Splat : IDifferentiable
    {
        public float3 mean;
        public float3 scale;
        public float4 rotate;
    };

    public struct SplatBuffer
    {
        StructuredBuffer<float4> rotates;
        StructuredBuffer<float4> means_scaleXs;
        StructuredBuffer<float2> scaleYZs;

        public Splat load(uint splatID)
        {
            Splat splat;
            float4 mean_scaleX = means_scaleXs[splatID];
            float2 scaleYZ = scaleYZs[splatID];
            float4 rotate = rotates[splatID];
            splat.mean = mean_scaleX.xyz;
            splat.scale = float3(mean_scaleX.w, scaleYZ);
            splat.rotate = rotate;
            return splat;
        }
    };

    public struct RWSplatBuffer
    {
        RWStructuredBuffer<float4> rotates;
        RWStructuredBuffer<float4> means_scaleXs;
        RWStructuredBuffer<float2> scaleYZs;

        public void store(uint splatID, in const Splat splat)
        {
            rotates[splatID] = splat.rotate;
            means_scaleXs[splatID] = float4(splat.mean, splat.scale.x);
            scaleYZs[splatID] = splat.scale.yz;
        }
    };

    public struct RWSplatAdamBuffer
    {
        RWStructuredBuffer<float4> rotateMs;
        RWStructuredBuffer<float4> meanMs_scaleXMs;
        RWStructuredBuffer<float4> scaleYZMs_scaleYZVs;
        RWStructuredBuffer<float4> rotateVs;
        RWStructuredBuffer<float4> meanVs_scaleXVs;
    };

    public struct SplatViewGeom : IDifferentiable
    {
        public float2 clipMean;
        public float2 axis;
        public float2 scale;
    };

    public struct SplatChannel : IDifferentiable
    {
        public float depth;
    }

    public struct SplatView : IDifferentiable
    {
        public SplatViewGeom geom;
        public SplatChannel channel;
    };

    public struct SplatViewBuffer
    {
        StructuredBuffer<float4> clipMeans_depths;
        StructuredBuffer<float4> axes_scales;

        public SplatView load(uint splatViewID)
        {
            float4 clipMean_depth = clipMeans_depths[splatViewID];
            float4 axis_scale = axes_scales[splatViewID];
            SplatView splatView;
            splatView.geom.clipMean = clipMean_depth.xy;
            splatView.channel.depth = clipMean_depth.z;
            splatView.geom.axis = axis_scale.xy;
            splatView.geom.scale = axis_scale.zw;
            return splatView;
        }
    };

    public struct RWSplatViewBuffer
    {
        RWStructuredBuffer<float4> clipMeans_depths;
        RWStructuredBuffer<float4> axes_scales;

        public void store(uint splatViewID, in const SplatView splatView)
        {
            clipMeans_depths[splatViewID] = float4(splatView.geom.clipMean, splatView.channel.depth, 0);
            axes_scales[splatViewID] = float4(splatView.geom.axis, splatView.geom.scale);
        }
    };

    public struct SplatRTTexture
    {
        Texture2D<float> depths, Ts;

        public SplatChannel load(uint2 coord, out float o_T)
        {
            SplatChannel ch;
            ch.depth = depths[coord].x;
            o_T = Ts[coord].x;
            return ch;
        }
    };

    public struct SplatRTOut
    {
        public float4 depth : SV_Target0;
        public float4 alpha : SV_Target1;

        public static SplatRTOut create(float depth, float alpha)
        {
            SplatRTOut o;
            o.depth = float4(depth, depth, depth, alpha);
            o.alpha = float4(1, 1, 1, alpha);
            return o;
        }
    };

    public struct MeshRTTexture
    {
        Texture2D<float> depths;

        public SplatChannel load(uint2 coord)
        {
            float depth = depths[coord];
            SplatChannel ch;
            ch.depth = depth;
            return ch;
        }
    };

    public struct MeshRTOut
    {
        public float4 depth : SV_Target0;

        public static MeshRTOut create(float depth)
        {
            MeshRTOut o;
            o.depth = float4(depth, depth, depth, 1);
            return o;
        }
    };

    public struct SplatTexture
    {
        Texture2D<float2> depths_Ts;

        public SplatChannel load(uint2 coord, out float o_T)
        {
            float2 depth_T = depths_Ts[coord];
            SplatChannel ch;
            ch.depth = depth_T.x;
            o_T = depth_T.y;
            return ch;
        }
    };

    /* struct Pixel
    {
        float depth;
        float prodT;
    };

    struct PixelImage
    {
        Texture2D<float2> depths_prodTs;
        Pixel load(uint2 coord) {
            float2 depth_prodT =
        }
    } */

    [Differentiable]
    public float viewZ2Depth(float viewZ, no_diff float camNearZ, no_diff float camFarZ)
    {
        // Use inversed viewZ as depth
        return camNearZ / -viewZ;
    }

    [Differentiable]
    public SplatView splat2splatView(
        Splat splat,
        float3 splatViewPos,
        float3 splatClipPos,
        no_diff float4x4 camViewMat,
        no_diff float4x4 camProjMat,
        no_diff float camNearZ,
        no_diff float camFarZ,
        no_diff float2 camResolution
    )
    {
        float2x3 cov3d = GS3D::getCovariance3D(splat.rotate, splat.scale);
        float3 cov2d = GS3D::getCovariance2D(cov3d, splatViewPos, float3x3(camViewMat), camProjMat, camResolution);
        GS3D::Eigen<2> eigen2d = GS3D::decomposeCovariance2D(cov2d);
        SplatView splatView;
        splatView.geom.clipMean = splatClipPos.xy;
        splatView.channel.depth = viewZ2Depth(splatViewPos.z, camNearZ, camFarZ);
        splatView.geom.axis = eigen2d.eigen[0];
        splatView.geom.scale = sqrt(2.0 * eigen2d.lambda);
        // e^{-d^2} = e^{-\frac{1}{2}(\frac{a}{\sigma})^2} \Rightarrow a = \sqrt{2}\sigma d
        // Let d = 2, a = 2\sqrt{2} \sigma < 3 \sigma
        return splatView;
    }

    [Differentiable]
    public SplatView splat2splatView(
        Splat splat,
        no_diff float4x4 camViewMat,
        no_diff float4x4 camProjMat,
        no_diff float camNearZ,
        no_diff float camFarZ,
        no_diff float2 camResolution
    )
    {
        float3 viewPos = mul(camViewMat, float4(splat.mean, 1)).xyz;
        float4 clipPos = mul(camProjMat, float4(viewPos, 1));
        clipPos /= clipPos.w;
        return splat2splatView(splat, viewPos, clipPos.xyz, camViewMat, camProjMat, camNearZ, camFarZ, camResolution);
    }

    public float2x2 getDeltaClip(SplatViewGeom splatViewGeom, float2 camResolution)
    {
        float2x2 deltaScreen = float2x2(
            splatViewGeom.axis * splatViewGeom.scale[0],                                  //
            float2(-splatViewGeom.axis.y, splatViewGeom.axis.x) * splatViewGeom.scale[1], //
        );
        float2x2 deltaClip = deltaScreen;
        deltaClip[0] *= 2.0 / camResolution;
        deltaClip[1] *= 2.0 / camResolution;
        // * 2 to scale from [0, 1] to [-1, 1]
        return deltaClip;
    }

    [Differentiable]
    public float getAlpha(SplatViewGeom splatViewGeom, no_diff float2 screenPos, no_diff float2 camResolution)
    {
        // clipMean to screenMean
        float2 screenMean = (float2(0.5, -0.5) * splatViewGeom.clipMean + 0.5) * camResolution;
        float2 deltaScreen = screenPos - screenMean;
        deltaScreen.y = -deltaScreen.y;
        float2 quadPos =
            mul(float2x2(
                    splatViewGeom.axis / splatViewGeom.scale[0],                                  //
                    float2(-splatViewGeom.axis.y, splatViewGeom.axis.x) / splatViewGeom.scale[1], //
                ),
                deltaScreen);
        return kOpacity * exp(-dot(quadPos, quadPos));
    }
}

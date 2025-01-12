import GaussianGI.Algorithm.GS3D;

#include "MeshGSTrainer.slangh"
#include "Utils/Math/MathConstants.slangh"

namespace MeshGSTrainer
{
    public const static float kOpacity = 0.975;                   // O
    public const static float kAlphaThreshold = 1.0 / 255.0;      // \alpha_T
    public const static float kQuadPosBound = 2.3486050619834185; // Q
    // \alpha = O \exp{-Q^2} = \alpha_T
    // \Rightarrow Q = \sqrt{\log{\frac{O}{\alpha_T}}}

    public struct Camera
    {
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
        public float3 conic;
    };

    public struct SplatChannel : IDifferentiable
    {
        public float depth;

        public float sum()
        {
            return depth; // For BackwardDraw
        }
    };
    public SplatChannel operator +(SplatChannel l, SplatChannel r)
    {
        return SplatChannel(l.depth + r.depth);
    }
    public SplatChannel operator -(SplatChannel l, SplatChannel r)
    {
        return SplatChannel(l.depth - r.depth);
    }
    public SplatChannel operator*(float l, SplatChannel r)
    {
        return SplatChannel(l * r.depth);
    }
    public SplatChannel operator*(SplatChannel l, SplatChannel r)
    {
        return SplatChannel(l.depth * r.depth);
    }

    public struct SplatView : IDifferentiable
    {
        public SplatViewGeom geom;
        public SplatChannel channel;
    };

    public struct SplatViewBuffer
    {
        StructuredBuffer<float4> conics_depths;
        StructuredBuffer<float2> clipMeans;

        public SplatView load(uint splatViewID)
        {
            float4 conic_depth = conics_depths[splatViewID];
            float2 clipMean = clipMeans[splatViewID];
            SplatView splatView;
            splatView.geom.clipMean = clipMean;
            splatView.channel.depth = conic_depth.w;
            splatView.geom.conic = conic_depth.xyz;
            return splatView;
        }
    };

    public uint getCoordAtomicBinID(uint2 coord)
    {
        return (coord.x % DLOSS_ATOMIC_BIN_DIM_X) * DLOSS_ATOMIC_BIN_DIM_Y + (coord.y % DLOSS_ATOMIC_BIN_DIM_Y);
    }

    public struct RWSplatViewBuffer
    {
        RWStructuredBuffer<float4> conics_depths;
        RWStructuredBuffer<float2> clipMeans;

        public void store(uint splatViewID, in const SplatView splatView)
        {
            conics_depths[splatViewID] = float4(splatView.geom.conic, splatView.channel.depth);
            clipMeans[splatViewID] = splatView.geom.clipMean;
        }

        public void binAtomicAdd(uint splatViewID, uint binID, in const SplatView splatView)
        {
            uint splatViewBinID = splatViewID * DLOSS_ATOMIC_BIN_SIZE + binID;
            // if (splatView.geom.conic.x >= HLF_MIN)
            __atomicAdd(conics_depths[splatViewBinID].x, splatView.geom.conic.x);
            // if (splatView.geom.conic.y >= HLF_MIN)
            __atomicAdd(conics_depths[splatViewBinID].y, splatView.geom.conic.y);
            // if (splatView.geom.conic.z >= HLF_MIN)
            __atomicAdd(conics_depths[splatViewBinID].z, splatView.geom.conic.z);
            // if (splatView.channel.depth >= HLF_MIN)
            __atomicAdd(conics_depths[splatViewBinID].w, splatView.channel.depth);
            // if (splatView.geom.clipMean.x >= HLF_MIN)
            __atomicAdd(clipMeans[splatViewBinID].x, splatView.geom.clipMean.x);
            // if (splatView.geom.clipMean.y >= HLF_MIN)
            __atomicAdd(clipMeans[splatViewBinID].y, splatView.geom.clipMean.y);
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

    public struct CoherentRasterizerOrderedSplatTexture
    {
        globallycoherent RasterizerOrderedTexture2D<float2> depths_Ts;

        public SplatChannel load(uint2 coord, out float o_T)
        {
            float2 depth_T = depths_Ts[coord];
            SplatChannel ch;
            ch.depth = depth_T.x;
            o_T = depth_T.y;
            return ch;
        }
        public void store(uint2 coord, SplatChannel ch, float T) { depths_Ts[coord] = float2(ch.depth, T); }
    };

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
        no_diff out float2x2 o_screenAxis,
        no_diff float4x4 camViewMat,
        no_diff float4x4 camProjMat,
        no_diff float camNearZ,
        no_diff float camFarZ,
        no_diff float2 screenResolution
    )
    {
        float2x3 cov3d = GS3D::getCovariance3D(splat.rotate, splat.scale);
        float3 cov2d = GS3D::getCovariance2D(cov3d, splatViewPos, float3x3(camViewMat), camProjMat, screenResolution);
        GS3D::Eigen<2> eigen2d = GS3D::decomposeCovariance2D(cov2d);
        SplatView splatView;
        splatView.geom.clipMean = splatClipPos.xy;
        splatView.geom.conic = GS3D::getConic(cov2d);
        splatView.channel.depth = viewZ2Depth(splatViewPos.z, camNearZ, camFarZ);

        float2 screenScale = sqrt(2.0 * eigen2d.lambda);
        o_screenAxis = float2x2(eigen2d.eigen[0] * screenScale[0], eigen2d.eigen[1] * screenScale[1]);
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
        no_diff float2 screenResolution
    )
    {
        float3 viewPos = mul(camViewMat, float4(splat.mean, 1)).xyz;
        float4 clipPos = mul(camProjMat, float4(viewPos, 1));
        clipPos /= clipPos.w;
        float2x2 screenAxis;
        return splat2splatView(splat, viewPos, clipPos.xyz, screenAxis, camViewMat, camProjMat, camNearZ, camFarZ, screenResolution);
    }

    public float2x2 getDeltaClip(float2x2 screenAxis, float2 screenResolution)
    {
        float2x2 deltaClip = screenAxis;
        deltaClip[0] *= 2.0 / screenResolution;
        deltaClip[1] *= 2.0 / screenResolution;
        // * 2 to scale from [0, 1] to [-1, 1]
        return deltaClip;
    }

    // Deprecated, now we use conic instead of axis+scale to reduce atomic operations
    /* [Differentiable]
    public float getAlpha(SplatViewGeom splatViewGeom, no_diff float2 screenPos, no_diff float2 screenResolution)
    {
        // clipMean to screenMean
        float2 screenMean = (float2(0.5, -0.5) * splatViewGeom.clipMean + 0.5) * screenResolution;
        float2 deltaScreen = screenPos - screenMean;
        deltaScreen.y = -deltaScreen.y;
        float2 quadPos =
            mul(float2x2(
                    splatViewGeom.axis / splatViewGeom.scale[0],                                  //
                    float2(-splatViewGeom.axis.y, splatViewGeom.axis.x) / splatViewGeom.scale[1], //
                ),
                deltaScreen);
        return kOpacity * exp(-dot(quadPos, quadPos));
    } */

    [Differentiable]
    public float getAlpha(SplatViewGeom splatViewGeom, no_diff float2 screenPos, no_diff float2 screenResolution)
    {
        // clipMean to screenMean
        float2 screenMean = (float2(0.5, -0.5) * splatViewGeom.clipMean + 0.5) * screenResolution;
        float2 deltaScreen = screenPos - screenMean;
        deltaScreen.y = -deltaScreen.y;
        return kOpacity * exp(-0.5 * (splatViewGeom.conic.x * deltaScreen.x * deltaScreen.x + //
                                      splatViewGeom.conic.z * deltaScreen.y * deltaScreen.y) -
                              splatViewGeom.conic.y * deltaScreen.x * deltaScreen.y);
    }
}

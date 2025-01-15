import GaussianGI.Algorithm.GS3D;
import GaussianGI.Util.SOABufferUtil;

#include "MeshGSTrainer.slangh"
#include "Utils/Math/MathConstants.slangh"

#define SPLAT_FLT_COUNT DEPTH_TRAIN_TYPE_SPLAT_FLT_COUNT
#define SPLAT_VIEW_FLT_COUNT DEPTH_TRAIN_TYPE_SPLAT_VIEW_FLT_COUNT
#define SPLAT_ADAM_FLT_COUNT DEPTH_TRAIN_TYPE_SPLAT_ADAM_FLT_COUNT

namespace MeshGSTrainer
{
    typealias SplatSOAUnitTrait = SOAUnitTrait<float, 4>;

    typealias SplatSOATrait = SOATrait<SplatSOAUnitTrait, SPLAT_FLT_COUNT>;
    public typealias FlatSplat = SOATuple<SplatSOATrait>;
    public typealias SplatBuffer = SOABuffer<SplatSOATrait>;
    public typealias RWSplatBuffer = RWSOABuffer<SplatSOATrait>;

    typealias SplatViewSOATrait = SOATrait<SplatSOAUnitTrait, SPLAT_VIEW_FLT_COUNT>;
    public typealias FlatSplatView = SOATuple<SplatViewSOATrait>;
    public typealias SplatViewBuffer = SOABuffer<SplatViewSOATrait>;
    public typealias RWSplatViewBuffer = RWSOABuffer<SplatViewSOATrait>;

    typealias SplatAdamSOATrait = SOATrait<SplatSOAUnitTrait, SPLAT_ADAM_FLT_COUNT>;
    public typealias FlatSplatAdam = SOATuple<SplatAdamSOATrait>;
    public typealias SplatAdamBuffer = SOABuffer<SplatAdamSOATrait>;
    public typealias RWSplatAdamBuffer = RWSOABuffer<SplatAdamSOATrait>;

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

    public struct Splat : IDifferentiable, ISOAElement<SplatSOATrait>
    {
        public float4 rotate;
        public float3 mean;
        public float3 scale;

        public FlatSplat toSOA() { return FlatSplat( { rotate, float4(mean, scale.x) }, scale.yz); }
        public static Splat fromSOA(FlatSplat soa)
        {
            Splat splat;
            splat.rotate = soa.units[0];
            splat.mean = soa.units[1].xyz;
            splat.scale = float3(soa.units[1].w, soa.ext);
            return splat;
        }
    };

    public struct SplatAdam : ISOAElement<SplatAdamSOATrait>
    {
        public FlatSplat m, v;

        public FlatSplatAdam toSOA()
        {
            return FlatSplatAdam(
                { m.units[0], m.units[1], v.units[0], v.units[1] }, //
                float4(m.ext, v.ext)
            );
        }
        public static SplatAdam fromSOA(FlatSplatAdam soa)
        {
            SplatAdam splatAdam;
            splatAdam.m = FlatSplat( { soa.units[0], soa.units[1] }, soa.ext.xy);
            splatAdam.v = FlatSplat( { soa.units[2], soa.units[3] }, soa.ext.zw);
            return splatAdam;
        }
    }

    public struct SplatViewGeom : IDifferentiable
    {
        public float2 clipMean;
        public float3 conic;
    };

    public struct SplatChannel : IDifferentiable
    {
        public float depth;

        [Differentiable]
        public float sum() { return depth; }
        [Differentiable]
        public SplatChannel add(SplatChannel r) { return SplatChannel(this.depth + r.depth); }
        [Differentiable]
        public SplatChannel sub(SplatChannel r) { return SplatChannel(this.depth - r.depth); }
        [Differentiable]
        public SplatChannel mul(SplatChannel r) { return SplatChannel(this.depth * r.depth); }
        [Differentiable]
        public SplatChannel mul(float r) { return SplatChannel(this.depth * r); }
        [Differentiable]
        public SplatChannel abs() { return SplatChannel(abs(this.depth)); }
    };

    public struct SplatView : IDifferentiable, ISOAElement<SplatViewSOATrait>
    {
        public SplatViewGeom geom;
        public SplatChannel channel;

        public FlatSplatView toSOA() { return FlatSplatView( { float4(geom.conic, channel.depth) }, geom.clipMean); }
        public static SplatView fromSOA(FlatSplatView soa)
        {
            SplatView splatView;
            splatView.geom.conic = soa.units[0].xyz;
            splatView.channel.depth = soa.units[0].w;
            splatView.geom.clipMean = soa.ext;
            return splatView;
        }
    };

    public extension FlatSplatView
    {
        public This waveActiveSum()
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < This::kUnitsPerElem; ++i)
                t.units[i] = WaveActiveSum(this.units[i]);
            t.ext = WaveActiveSum(this.ext);
            return t;
        }
        public This quadSum()
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < This::kUnitsPerElem; ++i)
            {
                t.units[i] = this.units[i];
                t.units[i] += QuadReadAcrossX(t.units[i]);
                t.units[i] += QuadReadAcrossY(t.units[i]);
            }
            t.ext = this.ext;
            t.ext += QuadReadAcrossX(t.ext);
            t.ext += QuadReadAcrossY(t.ext);
            return t;
        }
    }

    public extension RWSplatViewBuffer
    {
        public void atomicAdd<T : ISOAElement<Trait>>(uint id, T elem)
        {
            var a = elem.toSOA();
            [ForceUnroll]
            for (uint i = 0; i < This::kUnitsPerElem; ++i)
            {
                [ForceUnroll]
                for (uint j = 0; j < This::kWordsPerUnit; ++j)
                    __atomicAdd(unitBufs[i][id][j], a.units[i][j]);
            }
            [ForceUnroll]
            for (uint j = 0; j < This::kWordsPerExt; ++j)
                __atomicAdd(extBuf[id][j], a.ext[j]);
        }
    }

    public extension FlatSplat
    {
#define FS_DEF_OP(NAME, OP)                            \
    public This NAME(This r)                           \
    {                                                  \
        This t;                                        \
        [ForceUnroll]                                  \
        for (uint i = 0; i < This::kUnitsPerElem; ++i) \
            t.units[i] = this.units[i] OP r.units[i];  \
        t.ext = this.ext OP r.ext;                     \
        return t;                                      \
    }                                                  \
    public This NAME(float r)                          \
    {                                                  \
        This t;                                        \
        [ForceUnroll]                                  \
        for (uint i = 0; i < This::kUnitsPerElem; ++i) \
            t.units[i] = this.units[i] OP r;           \
        t.ext = this.ext OP r;                         \
        return t;                                      \
    }

        FS_DEF_OP(add, +)
        FS_DEF_OP(sub, -)
        FS_DEF_OP(mul, *)
        FS_DEF_OP(div, /)
#undef FS_DEF_OP

        public This sqrt()
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < This::kUnitsPerElem; ++i)
                t.units[i] = sqrt(this.units[i]);
            t.ext = sqrt(this.ext);
            return t;
        }

        public bool anyIsNanOrInf()
        {
            [ForceUnroll]
            for (uint i = 0; i < This::kUnitsPerElem; ++i)
                if (any(isnan(this.units[i])) || any(isinf(this.units[i])))
                    return true;
            return any(isnan(this.ext)) || any(isinf(this.ext));
        }
    }

    /* public uint getCoordAtomicBinID(uint2 coord)
    {
        return (coord.x % DLOSS_ATOMIC_BIN_DIM_X) * DLOSS_ATOMIC_BIN_DIM_Y + (coord.y % DLOSS_ATOMIC_BIN_DIM_Y);
    } */

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

    public struct RWSplatTexture
    {
        RWTexture2D<float2> depths_Ts;

        public void store(uint2 coord, SplatChannel ch, float T) { depths_Ts[coord] = float2(ch.depth, T); }
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
        viewZ = -viewZ; // make viewZ positive
        return camNearZ / viewZ + 0.02;
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
    public float getAlphaSafe(
        SplatViewGeom splatViewGeom,
        no_diff float2 screenPos,
        no_diff float2 screenResolution,
        no_diff out bool o_shouldDiscard
    )
    {
        // clipMean to screenMean
        float2 screenMean = (float2(0.5, -0.5) * splatViewGeom.clipMean + 0.5) * screenResolution;
        float2 deltaScreen = screenPos - screenMean;
        deltaScreen.y = -deltaScreen.y;
        float power = -0.5 * (splatViewGeom.conic.x * deltaScreen.x * deltaScreen.x + //
                              splatViewGeom.conic.z * deltaScreen.y * deltaScreen.y) -
                      splatViewGeom.conic.y * deltaScreen.x * deltaScreen.y;
        float alpha = kOpacity * exp(power);
        o_shouldDiscard = power > 0.0 || alpha < kAlphaThreshold;
        return alpha;
    }

    [Differentiable]
    public float getAlpha(SplatViewGeom splatViewGeom, no_diff float2 screenPos, no_diff float2 screenResolution)
    {
        bool shouldDiscard;
        return getAlphaSafe(splatViewGeom, screenPos, screenResolution, shouldDiscard);
    }
}

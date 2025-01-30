import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Util.SOABufferUtil;

#include "MeshGSTrainer.slangh"
#include "Utils/Math/MathConstants.slangh"

#include "Trait/Depth.slangh"

namespace MeshGSTrain
{
    public struct Camera
    {
        public float4x4 viewMat, projMat;
        public float nearZ, farZ;
    }

    public const static float kOpacity = 0.975;                   // O
    public const static float kAlphaThreshold = 1.0 / 255.0;      // \alpha_T
    public const static float kQuadPosBound = 2.3486050619834185; // Q
                                                                  // \alpha = O \exp{-Q^2} = \alpha_T
                                                                  // \Rightarrow Q = \sqrt{\log{\frac{O}{\alpha_T}}}

    public extension Trait
    {
        static const uint kFloatsPerSplat = kFloatsPerSplatGeom + Trait::kFloatsPerSplatAttrib;
        typealias SplatSOATrait = SOATrait<float, 4, kFloatsPerSplat>;
        public typealias FlatSplat = SOATuple<SplatSOATrait>;
        public typealias SplatBuffer = SOABuffer<SplatSOATrait>;
        public typealias RWSplatBuffer = RWSOABuffer<SplatSOATrait>;

        static const uint kFloatsPerSplatView = kFloatsPerSplatViewGeom + Trait::kFloatsPerSplatChannel;
        typealias SplatViewSOATrait = SOATrait<float, 4, kFloatsPerSplatView>;
        public typealias FlatSplatView = SOATuple<SplatViewSOATrait>;
        public typealias SplatViewBuffer = SOABuffer<SplatViewSOATrait>;
        public typealias RWSplatViewBuffer = RWSOABuffer<SplatViewSOATrait>;

        static const uint kFloatsPerSplatAdam = kFloatsPerSplat * 2;
        typealias SplatAdamSOATrait = SOATrait<float, 4, kFloatsPerSplatAdam>;
        public typealias FlatSplatAdam = SOATuple<SplatAdamSOATrait>;
        public typealias SplatAdamBuffer = SOABuffer<SplatAdamSOATrait>;
        public typealias RWSplatAdamBuffer = RWSOABuffer<SplatAdamSOATrait>;

        public struct Splat : ISOAElement<SplatSOATrait>, IDifferentiable
        {
            public SplatGeom geom;
            public Trait::SplatAttrib attrib;

            public FlatSplat toSOA()
            {
                FlatSplat soa;
                soa.setWords(0, geom.rotate);
                soa.setWords(4, geom.mean);
                soa.setWords(7, geom.scale);
                [ForceUnroll]
                for (uint i = 0; i < Trait::kFloatsPerSplatAttrib; ++i)
                    soa.setWord(kFloatsPerSplatGeom + i, attrib.getWord(i));
                return soa;
            }
            public static This fromSOA(FlatSplat soa)
            {
                This splat;
                splat.geom.rotate = soa.getWords<4>(0);
                splat.geom.mean = soa.getWords<3>(4);
                splat.geom.scale = soa.getWords<3>(7);
                [ForceUnroll]
                for (uint i = 0; i < Trait::kFloatsPerSplatAttrib; ++i)
                    splat.attrib.setWord(i, soa.getWord(kFloatsPerSplatGeom + i));
                return splat;
            }
        }

        public struct SplatView : IDifferentiable, ISOAElement<SplatViewSOATrait>
        {
            public SplatViewGeom geom;
            public Trait::SplatChannel channel;

            public FlatSplatView toSOA()
            {
                FlatSplatView soa;
                soa.setWords(0, geom.clipMean);
                soa.setWords(2, geom.conic);
                [ForceUnroll]
                for (uint i = 0; i < Trait::kFloatsPerSplatChannel; ++i)
                    soa.setWord(kFloatsPerSplatViewGeom + i, channel.getWord(i));
                return soa;
            }
            public static This fromSOA(FlatSplatView soa)
            {
                This splatView;
                splatView.geom.clipMean = soa.getWords<2>(0);
                splatView.geom.conic = soa.getWords<3>(2);
                [ForceUnroll]
                for (uint i = 0; i < Trait::kFloatsPerSplatChannel; ++i)
                    splatView.channel.setWord(i, soa.getWord(kFloatsPerSplatViewGeom + i));
                return splatView;
            }
        }

        public struct SplatAdam : ISOAElement<SplatAdamSOATrait>
        {
            public FlatSplat m, v;

            public FlatSplatAdam toSOA()
            {
                FlatSplatAdam soa;
                [ForceUnroll]
                for (uint i = 0; i < kFloatsPerSplat; ++i)
                {
                    soa.setWord(i, m.getWord(i));
                    soa.setWord(i + kFloatsPerSplat, v.getWord(i));
                }
                return soa;
            }
            public static This fromSOA(FlatSplatAdam soa)
            {
                This splatAdam;
                [ForceUnroll]
                for (uint i = 0; i < kFloatsPerSplat; ++i)
                {
                    splatAdam.m.setWord(i, soa.getWord(i));
                    splatAdam.v.setWord(i, soa.getWord(i + kFloatsPerSplat));
                }
                return splatAdam;
            }
        }
    }

    public extension Trait::SplatChannel
    {
        [Differentiable]
        public float sum()
        {
            float a = 0;
            [ForceUnroll]
            for (uint i = 0; i < Trait::kFloatsPerSplatChannel; ++i)
                a += getWord(i);
            return a;
        }
        [Differentiable]
        public This add(This r)
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < Trait::kFloatsPerSplatChannel; ++i)
                t.setWord(i, getWord(i) + r.getWord(i));
            return t;
        }
        [Differentiable]
        public This sub(This r)
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < Trait::kFloatsPerSplatChannel; ++i)
                t.setWord(i, getWord(i) - r.getWord(i));
            return t;
        }
        [Differentiable]
        public This mul(This r)
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < Trait::kFloatsPerSplatChannel; ++i)
                t.setWord(i, getWord(i) * r.getWord(i));
            return t;
        }
        [Differentiable]
        public This mul(float r)
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < Trait::kFloatsPerSplatChannel; ++i)
                t.setWord(i, getWord(i) * r);
            return t;
        }
        [Differentiable]
        public This abs()
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < Trait::kFloatsPerSplatChannel; ++i)
                t.setWord(i, abs(getWord(i)));
            return t;
        }
    }

    public extension Trait::FlatSplatView
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

    public extension Trait::FlatSplat
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
    public extension Trait::RWSplatViewBuffer
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

    /* public struct CoherentRasterizerOrderedSplatTexture
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
    }; */

    [Differentiable]
    public Trait::SplatView splat2splatView(
        Trait::Splat splat,
        float3 splatViewPos,
        float3 splatClipPos,
        no_diff out float2x2 o_screenAxis,
        no_diff float4x4 camViewMat,
        no_diff float camProjMat00,
        no_diff float camNearZ,
        no_diff float camFarZ,
        no_diff float2 screenResolution
    )
    {
        float2x3 cov3d = GS3D::getCovariance3D(splat.geom.rotate, splat.geom.scale);
        float3 cov2d = GS3D::getCovariance2D(cov3d, splatViewPos, float3x3(camViewMat), camProjMat00, screenResolution);
        Trait::SplatView splatView;
        splatView.geom.clipMean = splatClipPos.xy;
        splatView.geom.conic = GS3D::getConic(cov2d);
        splatView.channel = Trait::getSplatChannel(
            splat.geom,
            splat.attrib,
            splatView.geom,
            splatViewPos,
            splatClipPos,
            cov3d,
            cov2d,
            camViewMat,
            camProjMat00,
            camNearZ,
            camFarZ,
            screenResolution
        );

        GS3D::Eigen<2> eigen2d = GS3D::decomposeCovariance2D(cov2d);
        float2 screenScale = sqrt(2.0 * eigen2d.lambda);
        o_screenAxis = float2x2(eigen2d.eigen[0] * screenScale[0], eigen2d.eigen[1] * screenScale[1]);
        // e^{-d^2} = e^{-\frac{1}{2}(\frac{a}{\sigma})^2} \Rightarrow a = \sqrt{2}\sigma d
        // Let d = 2, a = 2\sqrt{2} \sigma < 3 \sigma
        return splatView;
    }

    [Differentiable]
    public Trait::SplatView splat2splatView(
        Trait::Splat splat,
        no_diff float4x4 camViewMat,
        no_diff float4x4 camProjMat,
        no_diff float camNearZ,
        no_diff float camFarZ,
        no_diff float2 screenResolution
    )
    {
        float3 viewPos = mul(camViewMat, float4(splat.geom.mean, 1)).xyz;
        float4 clipPos = mul(camProjMat, float4(viewPos, 1));
        clipPos /= clipPos.w;

        float2x2 screenAxis;
        return splat2splatView(splat, viewPos, clipPos.xyz, screenAxis, camViewMat, camProjMat[0][0], camNearZ, camFarZ, screenResolution);
    }

    [Differentiable]
    public Trait::SplatView splat2splatViewGSPP(
        Trait::Splat splat,
        float3 splatViewPos,
        float3 splatClipPos,
        no_diff out float2x2 o_screenAxis,
        no_diff float4x4 camViewMat,
        no_diff float camProjMat00,
        no_diff float camNearZ,
        no_diff float camFarZ,
        no_diff float2 screenResolution
    )
    {
        float2x3 cov3d = GS3D::getCovariance3D(splat.geom.rotate, splat.geom.scale);
        float3 cov2d = GS3D::getGSPPCovariance2D(cov3d, splatViewPos, float3x3(camViewMat), camProjMat00, screenResolution);

        Trait::SplatView splatView;
        splatView.geom.clipMean = splatClipPos.xy;
        splatView.geom.conic = GS3D::getConic(cov2d);
        splatView.channel = Trait::getSplatChannel(
            splat.geom,
            splat.attrib,
            splatView.geom,
            splatViewPos,
            splatClipPos,
            cov3d,
            cov2d,
            camViewMat,
            camProjMat00,
            camNearZ,
            camFarZ,
            screenResolution
        );

        // Use naive covariance 2D for screenAxis
        GS3D::Eigen<2> eigen2d =
            GS3D::decomposeCovariance2D(GS3D::getCovariance2D(cov3d, splatViewPos, float3x3(camViewMat), camProjMat00, screenResolution));
        float2 screenScale = sqrt(2.0 * eigen2d.lambda);
        o_screenAxis = float2x2(eigen2d.eigen[0] * screenScale[0], eigen2d.eigen[1] * screenScale[1]);
        // e^{-d^2} = e^{-\frac{1}{2}(\frac{a}{\sigma})^2} \Rightarrow a = \sqrt{2}\sigma d
        // Let d = 2, a = 2\sqrt{2} \sigma < 3 \sigma

        return splatView;
    }

    [Differentiable]
    public Trait::SplatView splat2splatViewGSPP(
        Trait::Splat splat,
        no_diff float4x4 camViewMat,
        no_diff float4x4 camProjMat,
        no_diff float camNearZ,
        no_diff float camFarZ,
        no_diff float2 screenResolution
    )
    {
        float3 viewPos = mul(camViewMat, float4(splat.geom.mean, 1)).xyz;
        float4 clipPos = mul(camProjMat, float4(viewPos, 1));
        clipPos /= clipPos.w;

        float2x2 screenAxis;
        return splat2splatViewGSPP(
            splat, viewPos, clipPos.xyz, screenAxis, camViewMat, camProjMat[0][0], camNearZ, camFarZ, screenResolution
        );
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
    public float getAlphaGSPPSafe(
        SplatViewGeom splatViewGeom,
        no_diff float2 screenPos,
        no_diff float2 screenResolution,
        no_diff float camProjMat00,
        no_diff float4x4 camInvProjMat,
        no_diff out bool o_shouldDiscard
    )
    {
        float3 viewMeanDir = normalize(mul(camInvProjMat, float4(splatViewGeom.clipMean, 0, 1)).xyz);

        float2 clipPos = (screenPos / screenResolution) * 2.0 - 1.0;
        clipPos.y = -clipPos.y;
        float3 viewPosDir = mul(camInvProjMat, float4(clipPos, 0, 1)).xyz; // No need to normalize()

        float3x3 planeRotMat = getAlignRotMatT(viewMeanDir);
        float2 planePos = GS3D::project(mul(planeRotMat, viewPosDir), camProjMat00, screenResolution);

        float power = -0.5 * (splatViewGeom.conic.x * planePos.x * planePos.x + //
                              splatViewGeom.conic.z * planePos.y * planePos.y) -
                      splatViewGeom.conic.y * planePos.x * planePos.y;
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

    [Differentiable]
    public float getAlphaGSPP(
        SplatViewGeom splatViewGeom,
        no_diff float2 screenPos,
        no_diff float2 screenResolution,
        no_diff float camProjMat00,
        no_diff float4x4 camInvProjMat
    )
    {
        bool shouldDiscard;
        return getAlphaGSPPSafe(splatViewGeom, screenPos, screenResolution, camProjMat00, camInvProjMat, shouldDiscard);
    }
}

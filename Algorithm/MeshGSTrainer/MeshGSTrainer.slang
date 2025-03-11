import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.ShaderMisc;

#include "MeshGSTrainer.slangh"
#include "Utils/Math/MathConstants.slangh"

#ifndef TRAIT_INCLUDE_PATH
#define TRAIT_INCLUDE_PATH "GaussianGI/Algorithm/MeshGSTrainer/Trait/Depth.slangh"
#endif

#include TRAIT_INCLUDE_PATH

#define USE_N2_GENERALIZED_GAUSSIAN 0

namespace MeshGSTrain
{
    public struct Camera
    {
        public float4x4 viewMat, projMat;
        public float nearZ, farZ;
    }

    public const static float kOpacity = 0.975;              // O
    public const static float kAlphaThreshold = 1.0 / 255.0; // \alpha_T
#if USE_N2_GENERALIZED_GAUSSIAN
    public const static float kQuadPosBound = 1.5325159255235876;
#else
    public const static float kQuadPosBound = 2.3486050619834185; // Q
// \alpha = O \exp{-Q^2} = \alpha_T
// \Rightarrow Q = \sqrt{\log{\frac{O}{\alpha_T}}}
#endif

#define SOA_WORD_TYPE float
#define SOA_WORDS_PER_UNIT 4

#define FLOATS_PER_SPLAT (FLOATS_PER_SPLAT_GEOM + FLOATS_PER_SPLAT_ATTRIB)
#define FLOATS_PER_SPLAT_VIEW (FLOATS_PER_SPLAT_VIEW_GEOM + FLOATS_PER_SPLAT_CHANNEL)
#define FLOATS_PER_SPLAT_ADAM (FLOATS_PER_SPLAT * 2)
#define FLOATS_PER_SPLAT_CHANNEL_T (FLOATS_PER_SPLAT_CHANNEL + 1)

    // Splat SOA
#define SOA_WORDS_PER_ELEM FLOATS_PER_SPLAT
#define SOA_TUPLE_NAME FlatSplat
#define SOA_IELEMENT_NAME ISplatElement
#define SOA_BUFFER_NAME SplatBuffer
#define SOA_RWBUFFER_NAME RWSplatBuffer
#define SOA_BUFFER_DEF
#include "GaussianGI/Util/SOABufferDefine.slangh"
#undef SOA_BUFFER_DEF
    public extension FlatSplat
    {
#if SOA_HAS_UNITS && SOA_HAS_EXT
#define FS_DEF_OP(NAME, OP)                           \
    public This NAME(This r)                          \
    {                                                 \
        This t;                                       \
        [ForceUnroll]                                 \
        for (uint i = 0; i < SOA_UNITS_PER_ELEM; ++i) \
            t.units[i] = this.units[i] OP r.units[i]; \
        t.ext = this.ext OP r.ext;                    \
        return t;                                     \
    }                                                 \
    public This NAME(float r)                         \
    {                                                 \
        This t;                                       \
        [ForceUnroll]                                 \
        for (uint i = 0; i < SOA_UNITS_PER_ELEM; ++i) \
            t.units[i] = this.units[i] OP r;          \
        t.ext = this.ext OP r;                        \
        return t;                                     \
    }
#elif SOA_HAS_UNITS
#define FS_DEF_OP(NAME, OP)                           \
    public This NAME(This r)                          \
    {                                                 \
        This t;                                       \
        [ForceUnroll]                                 \
        for (uint i = 0; i < SOA_UNITS_PER_ELEM; ++i) \
            t.units[i] = this.units[i] OP r.units[i]; \
        return t;                                     \
    }                                                 \
    public This NAME(float r)                         \
    {                                                 \
        This t;                                       \
        [ForceUnroll]                                 \
        for (uint i = 0; i < SOA_UNITS_PER_ELEM; ++i) \
            t.units[i] = this.units[i] OP r;          \
        return t;                                     \
    }
#elif SOA_HAS_EXT
#define FS_DEF_OP(NAME, OP)        \
    public This NAME(This r)       \
    {                              \
        This t;                    \
        t.ext = this.ext OP r.ext; \
        return t;                  \
    }                              \
    public This NAME(float r)      \
    {                              \
        This t;                    \
        t.ext = this.ext OP r;     \
        return t;                  \
    }
#endif

        FS_DEF_OP(add, +)
        FS_DEF_OP(sub, -)
        FS_DEF_OP(mul, *)
        FS_DEF_OP(div, /)
#undef FS_DEF_OP

        public This sqrt()
        {
            This t;
#if SOA_HAS_UNITS
            [ForceUnroll]
            for (uint i = 0; i < SOA_UNITS_PER_ELEM; ++i)
                t.units[i] = sqrt(this.units[i]);
#endif
#if SOA_HAS_EXT
            t.ext = sqrt(this.ext);
#endif
            return t;
        }

        public bool anyIsNanOrInf()
        {
#if SOA_HAS_UNITS
            [ForceUnroll]
            for (uint i = 0; i < SOA_UNITS_PER_ELEM; ++i)
                if (any(isnan(this.units[i])) || any(isinf(this.units[i])))
                    return true;
#endif
#if SOA_HAS_EXT
            return any(isnan(this.ext)) || any(isinf(this.ext));
#endif
            return false;
        }
    }
#define SOA_BUFFER_UNDEF
#include "GaussianGI/Util/SOABufferDefine.slangh"
#undef SOA_BUFFER_UNDEF
#undef SOA_WORDS_PER_ELEM
#undef SOA_TUPLE_NAME
#undef SOA_IELEMENT_NAME
#undef SOA_BUFFER_NAME
#undef SOA_RWBUFFER_NAME

    // SplatView SOA
#define SOA_WORDS_PER_ELEM FLOATS_PER_SPLAT_VIEW
#define SOA_TUPLE_NAME FlatSplatView
#define SOA_IELEMENT_NAME ISplatViewElement
#define SOA_BUFFER_NAME SplatViewBuffer
#define SOA_RWBUFFER_NAME RWSplatViewBuffer
#define SOA_BUFFER_DEF
#include "GaussianGI/Util/SOABufferDefine.slangh"
#undef SOA_BUFFER_DEF
    public extension FlatSplatView
    {
        public This waveActiveSum()
        {
            This t;
#if SOA_HAS_UNITS
            [ForceUnroll]
            for (uint i = 0; i < SOA_UNITS_PER_ELEM; ++i)
                t.units[i] = WaveActiveSum(this.units[i]);
#endif

#if SOA_HAS_EXT
            t.ext = WaveActiveSum(this.ext);
#endif
            return t;
        }
        public This quadSum()
        {
            This t;
#if SOA_HAS_UNITS
            [ForceUnroll]
            for (uint i = 0; i < SOA_UNITS_PER_ELEM; ++i)
            {
                t.units[i] = this.units[i];
                t.units[i] += QuadReadAcrossX(t.units[i]);
                t.units[i] += QuadReadAcrossY(t.units[i]);
            }
#endif

#if SOA_HAS_EXT
            t.ext = this.ext;
            t.ext += QuadReadAcrossX(t.ext);
            t.ext += QuadReadAcrossY(t.ext);
#endif
            return t;
        }
    }
    public extension RWSplatViewBuffer
    {
        public void atomicAdd<T : ISplatViewElement>(uint id, T elem)
        {
            var a = elem.toSOA();
#if SOA_HAS_UNITS
            [ForceUnroll]
            for (uint i = 0; i < SOA_UNITS_PER_ELEM; ++i)
            {
                [ForceUnroll]
                for (uint j = 0; j < SOA_WORDS_PER_UNIT; ++j)
                    __atomic_add(unitBufs[i][id][j], a.units[i][j], MemoryOrder.Relaxed);
            }
#endif

#if SOA_HAS_EXT
            [ForceUnroll]
            for (uint j = 0; j < SOA_WORDS_PER_EXT; ++j)
                __atomic_add(extBuf[id][j], a.ext[j], MemoryOrder.Relaxed);
#endif
        }
    }
#define SOA_BUFFER_UNDEF
#include "GaussianGI/Util/SOABufferDefine.slangh"
#undef SOA_BUFFER_UNDEF
#undef SOA_WORDS_PER_ELEM
#undef SOA_TUPLE_NAME
#undef SOA_IELEMENT_NAME
#undef SOA_BUFFER_NAME
#undef SOA_RWBUFFER_NAME

    // SplatAdam SOA
#define SOA_WORDS_PER_ELEM FLOATS_PER_SPLAT_ADAM
#define SOA_TUPLE_NAME FlatSplatAdam
#define SOA_IELEMENT_NAME ISplatAdamElement
#define SOA_RWBUFFER_NAME RWSplatAdamBuffer
#define SOA_BUFFER_DEF
#define SOA_BUFFER_UNDEF
#include "GaussianGI/Util/SOABufferDefine.slangh"
#undef SOA_BUFFER_UNDEF
#undef SOA_BUFFER_DEF
#undef SOA_WORDS_PER_ELEM
#undef SOA_TUPLE_NAME
#undef SOA_IELEMENT_NAME
#undef SOA_RWBUFFER_NAME

    // SplatChannelT SOA
#define SOA_WORDS_PER_ELEM FLOATS_PER_SPLAT_CHANNEL_T
#define SOA_TUPLE_NAME FlatSplatChannelT
#define SOA_IELEMENT_NAME ISplatChannelTElement
#define SOA_TEXTURE2D_NAME SplatTexture
#define SOA_RWTEXTURE2D_NAME RWSplatTexture
#define SOA_COHERENTTEXTURE2D_NAME CoherentSplatTexture
#define SOA_BUFFER_DEF
#define SOA_BUFFER_UNDEF
#include "GaussianGI/Util/SOABufferDefine.slangh"
#undef SOA_BUFFER_UNDEF
#undef SOA_BUFFER_DEF
#undef SOA_WORDS_PER_ELEM
#undef SOA_TUPLE_NAME
#undef SOA_IELEMENT_NAME
#undef SOA_TEXTURE2D_NAME
#undef SOA_RWTEXTURE2D_NAME
#undef SOA_COHERENTTEXTURE2D_NAME

    public struct Splat : ISplatElement, IDifferentiable
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
            for (uint i = 0; i < FLOATS_PER_SPLAT_ATTRIB; ++i)
                soa.setWord(FLOATS_PER_SPLAT_GEOM + i, attrib.getWord(i));
            return soa;
        }
        public static This fromSOA(FlatSplat soa)
        {
            This splat;
            splat.geom.rotate = soa.getWords<4>(0);
            splat.geom.mean = soa.getWords<3>(4);
            splat.geom.scale = soa.getWords<3>(7);
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_ATTRIB; ++i)
                splat.attrib.setWord(i, soa.getWord(FLOATS_PER_SPLAT_GEOM + i));
            return splat;
        }
    }

    public struct SplatView : ISplatViewElement, IDifferentiable
    {
        public SplatViewGeom geom;
        public Trait::SplatChannel channel;

        public FlatSplatView toSOA()
        {
            FlatSplatView soa;
            soa.setWords(0, geom.clipMean);
            soa.setWords(2, geom.conic);
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_CHANNEL; ++i)
                soa.setWord(FLOATS_PER_SPLAT_VIEW_GEOM + i, channel.getWord(i));
            return soa;
        }
        public static This fromSOA(FlatSplatView soa)
        {
            This splatView;
            splatView.geom.clipMean = soa.getWords<2>(0);
            splatView.geom.conic = soa.getWords<3>(2);
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_CHANNEL; ++i)
                splatView.channel.setWord(i, soa.getWord(FLOATS_PER_SPLAT_VIEW_GEOM + i));
            return splatView;
        }
    }

    public struct SplatAdam : ISplatAdamElement
    {
        public FlatSplat m, v;

        public FlatSplatAdam toSOA()
        {
            FlatSplatAdam soa;
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT; ++i)
            {
                soa.setWord(i, m.getWord(i));
                soa.setWord(i + FLOATS_PER_SPLAT, v.getWord(i));
            }
            return soa;
        }
        public static This fromSOA(FlatSplatAdam soa)
        {
            This splatAdam;
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT; ++i)
            {
                splatAdam.m.setWord(i, soa.getWord(i));
                splatAdam.v.setWord(i, soa.getWord(i + FLOATS_PER_SPLAT));
            }
            return splatAdam;
        }
    }

    public struct SplatChannelT : ISplatChannelTElement
    {
        public Trait::SplatChannel channel;
        public float T;

        public FlatSplatChannelT toSOA()
        {
            FlatSplatChannelT soa;
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_CHANNEL; ++i)
                soa.setWord(i, this.channel.getWord(i));
            soa.setWord(FLOATS_PER_SPLAT_CHANNEL, this.T);
            return soa;
        }
        public static This fromSOA(FlatSplatChannelT soa)
        {
            This channelT;
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_CHANNEL; ++i)
                channelT.channel.setWord(i, soa.getWord(i));
            channelT.T = soa.getWord(FLOATS_PER_SPLAT_CHANNEL);
            return channelT;
        }
    }

    public extension Trait::SplatChannel
    {
        public float sum()
        {
            float a = 0;
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_CHANNEL; ++i)
                a += getWord(i);
            return a;
        }
        public This add(This r)
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_CHANNEL; ++i)
                t.setWord(i, getWord(i) + r.getWord(i));
            return t;
        }
        public This sub(This r)
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_CHANNEL; ++i)
                t.setWord(i, getWord(i) - r.getWord(i));
            return t;
        }
        public This mul(This r)
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_CHANNEL; ++i)
                t.setWord(i, getWord(i) * r.getWord(i));
            return t;
        }
        public This mul(float r)
        {
            This t;
            [ForceUnroll]
            for (uint i = 0; i < FLOATS_PER_SPLAT_CHANNEL; ++i)
                t.setWord(i, getWord(i) * r);
            return t;
        }
    }

    [Differentiable]
    public SplatView splat2splatView(
        Splat splat,
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
        SplatView splatView;
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
    public SplatView splat2splatView(
        Splat splat,
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
    public SplatView splat2splatViewGSPP(
        Splat splat,
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

        SplatView splatView;
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
    public SplatView splat2splatViewGSPP(
        Splat splat,
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
        float power = 0.5 * (splatViewGeom.conic.x * deltaScreen.x * deltaScreen.x + //
                             splatViewGeom.conic.z * deltaScreen.y * deltaScreen.y) +
                      splatViewGeom.conic.y * deltaScreen.x * deltaScreen.y;
#if USE_N2_GENERALIZED_GAUSSIAN
        float alpha = kOpacity * exp(-power * power);
#else
        float alpha = kOpacity * exp(-power);
#endif
        o_shouldDiscard = power < 0.0 || alpha < kAlphaThreshold;
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

        float power = 0.5 * (splatViewGeom.conic.x * planePos.x * planePos.x + //
                             splatViewGeom.conic.z * planePos.y * planePos.y) +
                      splatViewGeom.conic.y * planePos.x * planePos.y;
#if USE_N2_GENERALIZED_GAUSSIAN
        float alpha = kOpacity * exp(-power * power);
#else
        float alpha = kOpacity * exp(-power);
#endif
        o_shouldDiscard = power < 0.0 || alpha < kAlphaThreshold;
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

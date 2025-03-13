import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.ShaderMisc;

#include "MeshGSTrainer.slangh"
#include "Utils/Math/MathConstants.slangh"

#ifndef TRAIT_INCLUDE_PATH
#define TRAIT_INCLUDE_PATH "GaussianGI/Algorithm/MeshGSTrainer/Trait/Depth.slangh"
#endif

#include TRAIT_INCLUDE_PATH

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

#define SOA_WORD_TYPE float
#define SOA_WORDS_PER_UNIT 4

#define FLOATS_PER_SPLAT (FLOATS_PER_SPLAT_GEOM + FLOATS_PER_SPLAT_ATTRIB)
#define FLOATS_PER_SPLAT_VIEW (FLOATS_PER_SPLAT_VIEW_GEOM + FLOATS_PER_SPLAT_CHANNEL)
#define FLOATS_PER_SPLAT_ADAM (FLOATS_PER_SPLAT * 2)
#define FLOATS_PER_SPLAT_CHANNEL_T (FLOATS_PER_SPLAT_CHANNEL + 1)
#define FLOATS_PER_SPLAT_QUAD 6

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

    // SplatQuad SOA
#define SOA_WORDS_PER_ELEM FLOATS_PER_SPLAT_QUAD
#define SOA_TUPLE_NAME FlatSplatQuad
#define SOA_IELEMENT_NAME ISplatQuadElement
#define SOA_BUFFER_NAME SplatQuadBuffer
#define SOA_RWBUFFER_NAME RWSplatQuadBuffer
#define SOA_BUFFER_DEF
#define SOA_BUFFER_UNDEF
#include "GaussianGI/Util/SOABufferDefine.slangh"
#undef SOA_BUFFER_UNDEF
#undef SOA_BUFFER_DEF
#undef SOA_WORDS_PER_ELEM
#undef SOA_TUPLE_NAME
#undef SOA_IELEMENT_NAME
#undef SOA_RWBUFFER_NAME
#undef SOA_BUFFER_NAME

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

    public struct SplatChannelT : ISplatChannelTElement, IDifferentiable
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

    public extension Trait::MeshRTTexture
    {
        public SplatChannelT load(uint2 coord)
        {
            SplatChannelT channelT;
            channelT.channel = load(coord, channelT.T);
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

    public struct SplatQuad : ISplatQuadElement
    {
        public float3 delta0, delta1;

        public FlatSplatQuad toSOA()
        {
            FlatSplatQuad soa;
            soa.units[0] = float4(delta0, delta1.x);
            soa.ext = delta1.yz;
            return soa;
        }
        public static This fromSOA(FlatSplatQuad soa)
        {
            This quad;
            quad.delta0 = soa.units[0].xyz;
            quad.delta1 = float3(soa.units[0].w, soa.ext);
            return quad;
        }

        public static float3 getCenter(float2 clipMean, float4x4 camInvProjMat)
        {
            float3 center = normalize(mul(camInvProjMat, float4(clipMean, 0, 1)).xyz);
            return center;
        }
    }

    [Differentiable]
    public SplatView splat2splatView(
        Splat splat,
        float3 splatViewPos,
        float3 splatClipPos,
        no_diff out SplatQuad o_splatQuad,
        no_diff float4x4 camViewMat,
        no_diff float4x4 camProjMat,
        no_diff float camNearZ,
        no_diff float camFarZ
    )
    {
        float3x3 camViewRotMat = float3x3(camViewMat);

        float splatViewDist = length(splatViewPos);
        float3 splatViewDir = splatViewPos / splatViewDist;
        float3x3 splatViewDirMatT = getAlignRotMatT(splatViewDir);

        float2x3 cov3d = GS3D::getCovariance3D(splat.geom.rotate, splat.geom.scale);
        float3 cov2d = GS3D::getGSPPCovariance2D(cov3d, splatViewDist, mul(splatViewDirMatT, camViewRotMat), 1.0);

        SplatView splatView;
        splatView.geom.clipMean = splatClipPos.xy;
        splatView.geom.conic = GS3D::getConic(cov2d);
        splatView.channel = Trait::getSplatChannel(
            splat.geom, splat.attrib, splatView.geom, splatViewPos, splatClipPos, cov3d, cov2d, camViewMat, camProjMat, camNearZ, camFarZ
        );

        GS3D::Eigen<2> eigen = GS3D::decomposeCovariance2D(cov2d);

        // float4 centerClip = mul(camProjMat, float4(splatViewDir, 1.0));
        float2 deltaQuad0 = eigen.eigen[0] * sqrt(eigen.lambda[0] * 2.0);
        float2 deltaQuad1 = eigen.eigen[1] * sqrt(eigen.lambda[1] * 2.0);
        float3 delta0 = splatViewDirMatT[0] * deltaQuad0.x + splatViewDirMatT[1] * deltaQuad0.y;
        float3 delta1 = splatViewDirMatT[0] * deltaQuad1.x + splatViewDirMatT[1] * deltaQuad1.y;

        o_splatQuad = SplatQuad(delta0, delta1);

        return splatView;
    }

    [Differentiable]
    public SplatView splat2splatView(
        Splat splat,
        no_diff out SplatQuad o_splatQuad,
        no_diff float4x4 camViewMat,
        no_diff float4x4 camProjMat,
        no_diff float camNearZ,
        no_diff float camFarZ
    )
    {
        float3 viewPos = mul(camViewMat, float4(splat.geom.mean, 1)).xyz;
        float4 clipPos = mul(camProjMat, float4(viewPos, 1));
        clipPos /= clipPos.w;

        SplatQuad quad;
        return splat2splatView(splat, viewPos, clipPos.xyz, quad, camViewMat, camProjMat, camNearZ, camFarZ);
    }

    public float getAlpha(float2 splatQuadPos)
    {
        float power = dot(splatQuadPos, splatQuadPos);
        float alpha = kOpacity * exp(-power);
        return alpha;
    }

    [Differentiable]
    public float getAlpha(
        SplatViewGeom splatViewGeom,
        no_diff float3 fragViewDir, // No need to normalize()
        no_diff float4x4 camInvProjMat
    )
    {
        /* float2 fragClipPos = (fragScreenPos / screenResolution) * 2.0 - 1.0;
        fragClipPos.y = -fragClipPos.y;
        float3 fragViewDir = mul(camInvProjMat, float4(fragClipPos, 0, 1)).xyz; // No need to normalize() */

        float3 splatViewDir = normalize(mul(camInvProjMat, float4(splatViewGeom.clipMean, 0, 1)).xyz);
        float3x3 splatViewDirMatT = getAlignRotMatT(splatViewDir);
        float2 planePos = GS3D::project(mul(splatViewDirMatT, fragViewDir), 1.0);

        float power = 0.5 * (splatViewGeom.conic.x * planePos.x * planePos.x + //
                             splatViewGeom.conic.z * planePos.y * planePos.y) +
                      splatViewGeom.conic.y * planePos.x * planePos.y;
        float alpha = kOpacity * exp(-power);
        return alpha;
    }

    [Differentiable]
    public float loss(SplatChannelT predict, no_diff SplatChannelT data)
    {
        return Trait::loss(predict.channel, predict.T, data.channel, data.T);
    }
}

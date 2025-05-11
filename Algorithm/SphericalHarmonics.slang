#include "Utils/Math/MathConstants.slangh"
#include "SphericalHarmonics.slangh"

public struct SH<T : IFloat, let L_V : uint>
{
    public Array<T, SH_SIZE(L_V)> data;
}

public typealias SH1<T : IFloat> = SH<T, 0>;
public typealias SH4<T : IFloat> = SH<T, 1>;
public typealias SH9<T : IFloat> = SH<T, 2>;
public typealias SH16<T : IFloat> = SH<T, 3>;
public typealias SH25<T : IFloat> = SH<T, 4>;

public T shDot<T : IFloat, let L_V : uint>(SH<T, L_V> lSh, SH<float, L_V> rSh)
{
    T r = (T)0;
    [ForceUnroll]
    for (uint i = 0; i < SH_SIZE(L_V); ++i)
        r = r + lSh.data[i] * T(rSh.data[i]);
    return r;
}

public SH<T, L_V> shConvolve<T : IFloat, let L_V : uint>(SH<T, L_V> lSh, SH<float, L_V> rSh)
{
    [ForceUnroll]
    for (uint i = 0; i < SH_SIZE(L_V); ++i)
        lSh.data[i] = lSh.data[i] * T(rSh.data[i]);
    return lSh;
}

public SH<float3, L_V> shToFloat32<let L_V : uint>(SH<float16_t3, L_V> sh)
{
    SH<float3, L_V> dstSh;
    [ForceUnroll]
    for (uint i = 0; i < SH_SIZE(L_V); ++i)
        dstSh.data[i] = float3(sh.data[i]);
    return dstSh;
}

public SH<float16_t3, L_V> shToFloat16<let L_V : uint>(SH<float3, L_V> sh)
{
    SH<float16_t3, L_V> dstSh;
    [ForceUnroll]
    for (uint i = 0; i < SH_SIZE(L_V); ++i)
        dstSh.data[i] = float16_t3(sh.data[i]);
    return dstSh;
}

public SH<float, L_V> shBasis<let L_V : uint>(float3 p)
{
    // Evaluate SH basis
    // From Peter-Pike Sloan's Stupid SH Tricks
    // http://www.ppsloan.org/publications/StupidSH36.pdf
    // https://github.com/dariomanesku/cmft/blob/master/src/cmft/cubemapfilter.cpp#L130

    SH<float, L_V> r;

    float x = -p.x;
    float y = -p.y;
    float z = p.z;

    float x2 = x * x;
    float y2 = y * y;
    float z2 = z * z;

    float z3 = z2 * z;

    float x4 = x2 * x2;
    float y4 = y2 * y2;
    float z4 = z2 * z2;

    // r.data[0] = 1.0 / (2.0 * sqrtPi);
    r.data[0] = 0.28209479177387814;

    if (L_V >= 1)
    {
        /* r.data[1] = -sqrt(3.0 / (4.0 * pi)) * y;
        r.data[2] = sqrt(3.0 / (4.0 * pi)) * z;
        r.data[3] = -sqrt(3.0 / (4.0 * pi)) * x; */

        r.data[1] = -0.4886025119029199 * y;
        r.data[2] = 0.4886025119029199 * z;
        r.data[3] = -0.4886025119029199 * x;
    }

    if (L_V >= 2)
    {
        /* r.data[4] = sqrt(15.0 / (4.0 * pi)) * y * x;
        r.data[5] = -sqrt(15.0 / (4.0 * pi)) * y * z;
        r.data[6] = sqrt(5.0 / (16.0 * pi)) * (3.0 * z2 - 1.0);
        r.data[7] = -sqrt(15.0 / (4.0 * pi)) * x * z;
        r.data[8] = sqrt(15.0 / (16.0 * pi)) * (x2 - y2); */

        r.data[4] = 1.0925484305920792 * y * x;
        r.data[5] = -1.0925484305920792 * y * z;
        r.data[6] = 0.31539156525252005 * (3.0 * z2 - 1.0);
        r.data[7] = -1.0925484305920792 * x * z;
        r.data[8] = 0.5462742152960396 * (x2 - y2);
    }

    if (L_V >= 3)
    {
        /* r.data[9] = -sqrt(70.0 / (64.0 * pi)) * y * (3.0 * x2 - y2);
        r.data[10] = sqrt(105.0 / (4.0 * pi)) * y * x * z;
        r.data[11] = -sqrt(21.0 / (16.0 * pi)) * y * (-1.0 + 5.0 * z2);
        r.data[12] = sqrt(7.0 / (16.0 * pi)) * (5.0 * z3 - 3.0 * z);
        r.data[13] = -sqrt(21.0 / (64.0 * pi)) * x * (-1.0 + 5.0 * z2);
        r.data[14] = sqrt(105.0 / (16.0 * pi)) * (x2 - y2) * z;
        r.data[15] = -sqrt(70.0 / (64.0 * pi)) * x * (x2 - 3.0 * y2); */

        r.data[9] = -0.5900435899266435 * y * (3.0 * x2 - y2);
        r.data[10] = 2.890611442640554 * y * x * z;
        r.data[11] = -0.6463603682283013 * y * (-1.0 + 5.0 * z2);
        r.data[12] = 0.3731763325901154 * (5.0 * z3 - 3.0 * z);
        r.data[13] = -0.32318018411415067 * x * (-1.0 + 5.0 * z2);
        r.data[14] = 1.445305721320277 * (x2 - y2) * z;
        r.data[15] = -0.5900435899266435 * x * (x2 - 3.0 * y2);
    }

    if (L_V >= 4)
    {
        /* r.data[16] = 3.0 * sqrt(35.0 / (16.0 * pi)) * x * y * (x2 - y2);
        r.data[17] = -3.0 * sqrt(70.0 / (64.0 * pi)) * y * z * (3.0 * x2 - y2);
        r.data[18] = 3.0 * sqrt(5.0 / (16.0 * pi)) * y * x * (-1.0 + 7.0 * z2);
        r.data[19] = -3.0 * sqrt(10.0 / (64.0 * pi)) * y * z * (-3.0 + 7.0 * z2);
        r.data[20] = (105.0 * z4 - 90.0 * z2 + 9.0) / (16.0 * sqrtPi);
        r.data[21] = -3.0 * sqrt(10.0 / (64.0 * pi)) * x * z * (-3.0 + 7.0 * z2);
        r.data[22] = 3.0 * sqrt(5.0 / (64.0 * pi)) * (x2 - y2) * (-1.0 + 7.0 * z2);
        r.data[23] = -3.0 * sqrt(70.0 / (64.0 * pi)) * x * z * (x2 - 3.0 * y2);
        r.data[24] = 3.0 * sqrt(35.0 / (4.0 * (64.0 * pi))) * (x4 - 6.0 * y2 * x2 + y4); */

        r.data[16] = 2.5033429417967046 * x * y * (x2 - y2);
        r.data[17] = -1.7701307697799304 * y * z * (3.0 * x2 - y2);
        r.data[18] = 0.9461746957575601 * y * x * (-1.0 + 7.0 * z2);
        r.data[19] = -0.6690465435572892 * y * z * (-3.0 + 7.0 * z2);
        r.data[20] = 0.03526184897173477 * (105.0 * z4 - 90.0 * z2 + 9.0);
        r.data[21] = -0.6690465435572892 * x * z * (-3.0 + 7.0 * z2);
        r.data[22] = 0.47308734787878004 * (x2 - y2) * (-1.0 + 7.0 * z2);
        r.data[23] = -1.7701307697799304 * x * z * (x2 - 3.0 * y2);
        r.data[24] = 0.6258357354491761 * (x4 - 6.0 * y2 * x2 + y4);
    }

    return r;
}

public SH<float, L_V> shDiffuseLobe<let L_V : uint>()
{
    // https://cseweb.ucsd.edu/~ravir/papers/envmap/envmap.pdf equation 8

    SH<float, L_V> r;
    r.data[0] = M_PI;

    if (L_V >= 1)
        [ForceUnroll] for (uint i = 1; i < 4; ++i) r.data[i] = M_PI * 2.0 / 3.0;

    if (L_V >= 2)
        [ForceUnroll] for (uint i = 4; i < 9; ++i) r.data[i] = M_PI * 1.0 / 4.0;

    if (L_V >= 3)
        [ForceUnroll] for (uint i = 9; i < 16; ++i) r.data[i] = 0;

    if (L_V >= 4)
        [ForceUnroll] for (uint i = 16; i < 25; ++i) r.data[i] = -M_PI * 1.0 / 24.0;

    return r;
}

public T shDiffuse<T : IFloat, let L_V : uint>(SH<T, L_V> sh, SH<float, L_V> dirSh)
{
    SH<float, L_V> diffuseSh = shDiffuseLobe<L_V>();

    T r = (T)0;
    [ForceUnroll]
    for (uint i = 0; i < SH_SIZE(L_V); ++i)
        r = r + sh.data[i] * T(diffuseSh.data[i]) * T(dirSh.data[i]);
    return r;
}

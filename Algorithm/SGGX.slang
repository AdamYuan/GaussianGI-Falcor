import Utils.Math.MathHelpers;
import ShaderMisc;

public namespace SGGX
{
    public struct VNDFSampler
    {
        // public float3 w_i;
        public float3x3 w_kji;
        public float2x3 M_kji;

        public static VNDFSampler create(float2x3 cov3d, float sqrtDetCov3d, float3 w_i)
        {
            float3x3 S = float3x3(cov3d[0], float3(cov3d[0][1], cov3d[1][0], cov3d[1][1]), float3(cov3d[0][2], cov3d[1][1], cov3d[1][2]));
            float3x3 w_kji_T = getRobustAlignRotMatT(w_i), w_kji = transpose(w_kji_T);
            float3x3 S_kji = mul(w_kji_T, mul(S, w_kji));
            float S_kk = S_kji[0][0], S_kj = S_kji[0][1], S_ki = S_kji[0][2], S_jj = S_kji[1][1], S_ji = S_kji[1][2], S_ii = S_kji[2][2];
            float sqrt_det_Sji = sqrt(S_jj * S_ii - S_ji * S_ji);
            float inv_sqrt_det_Sji = 1.0 / sqrt_det_Sji;
            float inv_sqrt_Sii = 1.0 / sqrt(S_ii);
            float3 M_k = float3(sqrtDetCov3d * inv_sqrt_det_Sji, 0, 0);
            float3 M_j = inv_sqrt_Sii * float3(-(S_ki * S_ji - S_kj * S_ii) * inv_sqrt_det_Sji, sqrt_det_Sji, 0);
            float3 M_i = inv_sqrt_Sii * float3(S_ki, S_ji, S_ii);

            VNDFSampler sampler;
            sampler.w_kji = w_kji;
            sampler.M_kji = float2x3(float3(M_k.x, M_j.xy), M_i);
            return sampler;
        }

        public static VNDFSampler create(float3x3 rotMat, float3 scale, float3 w_i)
        {
            float3x3 scaleMat = float3x3(scale.x, 0, 0, 0, scale.y, 0, 0, 0, scale.z);
            float3x3 scaleRotMat = mul(rotMat, scaleMat);
            float3x3 S = mul(scaleRotMat, transpose(scaleRotMat));
            float2x3 cov3d = float2x3(S[0], float3(S[1].yz, S[2].z));
            float sqrtDetCov3d = scale.x * scale.y * scale.z;
            return create(cov3d, sqrtDetCov3d, w_i);
        }

        public static VNDFSampler create(float2x3 M_kji, float3 w_i)
        {
            VNDFSampler sampler;
            sampler.M_kji = M_kji;
            sampler.w_kji = transpose(getRobustAlignRotMatT(w_i));
            return sampler;
        }

        public static VNDFSampler fromSplat(float3x3 rotMat, float3 scale, float3 w_i) { return create(rotMat, 1.0 / scale, w_i); }

        public float3 apply(float3 d)
        {
            float3 w_m_kji = float3(this.M_kji[0].x * d.x, 0, 0) + float3(this.M_kji[0].yz * d.y, 0) + this.M_kji[1] * d.z;
            w_m_kji = normalize(w_m_kji);
            return mul(this.w_kji, w_m_kji);
        }

        // pdf equals to vndf
        public float3 sample(float2 u) { return apply(sample_cosine_hemisphere_polar(u)); }
    }

    public float3 getSplatNormal(float3x3 rotMat, float3 scale, float3 w_i)
    {
        VNDFSampler sampler = VNDFSampler::fromSplat(rotMat, scale, w_i);
        return sampler.apply(float3(0, 0, 1));
    }
}

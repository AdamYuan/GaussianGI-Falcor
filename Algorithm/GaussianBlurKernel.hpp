//
// Created by adamyuan on 2/26/25.
//

#ifndef GSGI_ALGO_GAUSSIANBLURKERNEL_HPP
#define GSGI_ALGO_GAUSSIANBLURKERNEL_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

struct GaussianBlurKernel
{
    static constexpr float kRadiusSigmaRatio = 2.0f;
    static constexpr uint32_t getKernelSize(uint32_t radius) { return radius + 1u; }

    uint32_t radius{};
    std::vector<float> weights; // .size() = radius + 1

    static GaussianBlurKernel create(uint32_t radius, float sigma)
    {
        GaussianBlurKernel kernel;
        kernel.radius = radius;
        kernel.weights.resize(getKernelSize(radius));

        const auto normpdf = [sigma](float x)
        {
            constexpr float k1_Sqrt2Pi = 0.3989422804014327f;
            return k1_Sqrt2Pi * math::exp(-0.5f * x * x / (sigma * sigma)) / sigma;
        };
        for (uint32_t x = 0; x <= radius; ++x)
            kernel.weights[x] = normpdf(float(x));

        return kernel;
    }
    static GaussianBlurKernel create(uint32_t radius) { return create(radius, float(radius) / kRadiusSigmaRatio); }
    static GaussianBlurKernel create(float sigma) { return create(uint(math::ceil(sigma * kRadiusSigmaRatio)), sigma); }

    void bindShaderWeights(const ShaderVar& var)
    {
        for (uint32_t i = 0; i < weights.size(); ++i)
            var[i] = weights[i];
    }
};

} // namespace GSGI

#endif

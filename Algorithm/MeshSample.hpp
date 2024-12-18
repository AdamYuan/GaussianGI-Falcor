//
// Created by adamyuan on 12/17/24.
//

#ifndef GSGI_MESHSAMPLER_HPP
#define GSGI_MESHSAMPLER_HPP

#include "MeshSample.hpp"

#include <Falcor.h>
#include <random>
#include "MeshView.hpp"
#include "AliasTable.hpp"

using namespace Falcor;

namespace GSGI
{

namespace Concepts
{
template<typename T>
concept MeshSampler = requires(T& t, uint2 u2, float2 f2) {
    { t(u2, f2) } -> std::same_as<void>;
};
} // namespace Concepts

template<typename Rand_T>
struct MeshSamplerDefault
{
    Rand_T rand;

    void operator()(uint2& u2, float2& f2)
    {
        std::uniform_int_distribution<uint32_t> uintDistrib{0u, 0xFFFFFFFFu};
        std::uniform_real_distribution<float> floatDistrib{0.0f, 1.0f};
        u2.x = uintDistrib(rand);
        u2.y = uintDistrib(rand);
        f2.x = floatDistrib(rand);
        f2.y = floatDistrib(rand);
    }
};

static_assert(Concepts::MeshSampler<MeshSamplerDefault<std::mt19937>>);

struct MeshSample
{
    uint32_t primitiveID;
    float2 barycentrics;

    float3 getPosition(Concepts::MeshView auto meshView) const
    {
        float3 p0 = meshView.getPrimitive(primitiveID).getVertex(0).getPosition();
        float3 p1 = meshView.getPrimitive(primitiveID).getVertex(1).getPosition();
        float3 p2 = meshView.getPrimitive(primitiveID).getVertex(2).getPosition();
        return p0 * (1.0f - barycentrics.x - barycentrics.y) + p1 * barycentrics.x + p2 * barycentrics.y;
    }

    static std::vector<MeshSample> sample(Concepts::MeshView auto meshView, Concepts::MeshSampler auto& sampler, uint32_t sampleCount)
    {
        AliasTable primitiveTable;
        {
            std::vector<float> primitiveAreas(meshView.getPrimitiveCount());
            for (uint primitiveID = 0; primitiveID < meshView.getPrimitiveCount(); ++primitiveID)
            {
                float3 p0 = meshView.getPrimitive(primitiveID).getVertex(0).getPosition();
                float3 p1 = meshView.getPrimitive(primitiveID).getVertex(1).getPosition();
                float3 p2 = meshView.getPrimitive(primitiveID).getVertex(2).getPosition();
                primitiveAreas[primitiveID] = math::length(math::cross(p1 - p0, p2 - p0)); //  * 0.5f;
            }
            primitiveTable = AliasTable::create(primitiveAreas);
        }
        return {};

        std::vector<MeshSample> samples(sampleCount);
        uint2 u2;
        float2 f2;
        for (auto& sample : samples)
        {
            sampler(u2, f2);
            sample.primitiveID = primitiveTable.sample(u2);
            if (f2.x + f2.y > 1.0f)
            {
                f2.x = 1.0f - f2.x;
                f2.y = 1.0f - f2.y;
            }
            sample.barycentrics = f2;
        }

        return samples;
    }
};

} // namespace GSGI

#endif // GSGI_MESHSAMPLER_HPP

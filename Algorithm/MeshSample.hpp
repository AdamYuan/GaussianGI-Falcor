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
    struct Result
    {
        std::vector<MeshPoint> points;
        float totalArea;
    };
    static Result sample(const Concepts::MeshView auto& meshView, Concepts::MeshSampler auto& sampler, uint32_t sampleCount)
    {
        float totalArea = 0;
        AliasTable primitiveTable;
        {
            std::vector<float> primitiveAreas(meshView.getPrimitiveCount());
            for (uint primitiveID = 0; primitiveID < meshView.getPrimitiveCount(); ++primitiveID)
            {
                auto [p0, p1, p2] = PrimitiveViewMethod::getVertexPositions(meshView.getPrimitive(primitiveID));
                float doubleArea = math::length(math::cross(p1 - p0, p2 - p0));
                totalArea += doubleArea;
                primitiveAreas[primitiveID] = doubleArea;
            }
            primitiveTable = AliasTable::create(primitiveAreas);
            totalArea *= 0.5f;
        }

        std::vector<MeshPoint> points(sampleCount);
        uint2 u2;
        float2 f2;
        for (auto& point : points)
        {
            sampler(u2, f2);
            point.primitiveID = primitiveTable.sample(u2);
            if (f2.x + f2.y > 1.0f)
            {
                f2.x = 1.0f - f2.x;
                f2.y = 1.0f - f2.y;
            }
            point.barycentrics = f2;
        }

        return {
            .points = std::move(points),
            .totalArea = totalArea,
        };
    }
};

} // namespace GSGI

#endif // GSGI_MESHSAMPLER_HPP

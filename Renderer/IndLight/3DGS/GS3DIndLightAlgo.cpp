//
// Created by adamyuan on 2/8/25.
//

#include "GS3DIndLightAlgo.hpp"

#include "../../../Scene/GMeshView.hpp"
#include "../../../Algorithm/MeshVHBVH.hpp"
#include "../../../Algorithm/MeshSample.hpp"
#include "../../../Algorithm/MeshGSOptimize.hpp"
#include <tbb/parallel_for.h>
#include <boost/random/sobol.hpp>
#include <random>

namespace GSGI
{

std::vector<GS3DIndLightSplat> GS3DIndLightAlgo::getSplatsFromMeshFallback(const ref<GMesh>& pMesh, uint splatCount)
{
    std::vector<GS3DIndLightSplat> meshSplats;
    auto view = GMeshView{pMesh};
    auto sampleResult = MeshSample::sample(
        view,
        [sobolEngine = boost::random::sobol_engine<uint32_t, 32>{4}] mutable
        {
            uint2 u2;
            float2 f2;
            u2.x = sobolEngine();
            u2.y = sobolEngine();
            f2.x = float(sobolEngine()) / 4294967296.0f;
            f2.y = float(sobolEngine()) / 4294967296.0f;
            return std::tuple{u2, f2};
        },
        splatCount
    );

    float initialScale = MeshGSOptimize::getInitialScale(sampleResult.totalArea, splatCount, 0.5f);
    logInfo("area: {}, initialScale: {}", sampleResult.totalArea, initialScale);

    auto bvh = MeshBVH<AABB>::build<MeshVHBVHBuilder>(view);
    meshSplats.resize(splatCount);

    tbb::parallel_for(
        uint32_t{0},
        splatCount,
        [&](uint32_t splatID)
        {
            meshSplats[splatID] = [&]
            {
                const auto& meshPoint = sampleResult.points[splatID];
                auto result = MeshGSOptimize::run(
                    view,
                    meshPoint,
                    bvh,
                    MeshGSSamplerDefault{std::mt19937{splatID}},
                    {
                        .initialScale = initialScale,
                        .sampleCount = 128,
                        .epsNormal = 0.2f,
                        .epsDistance = 0.2f * initialScale,
                        .epsScale = 1e-6f,
                        .scaleYMaxIteration = 32,
                    }
                );
                return GS3DIndLightSplat{
                    .mean = meshPoint.getPosition(view),
                    .rotate = float16_t4(result.rotate.x, result.rotate.y, result.rotate.z, result.rotate.w),
                    .scale = float16_t3(result.scaleXY, 0.1f * math::min(result.scaleXY.x, result.scaleXY.y)),
                    .irradiance = float16_t3{1.0f},
                };
            }();
        }
    );
    return meshSplats;
}

} // namespace GSGI
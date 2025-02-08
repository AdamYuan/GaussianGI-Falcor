//
// Created by adamyuan on 2/7/25.
//

#include "GS3DIndLightSplat.hpp"

#include "../../../Util/SerializeUtil.hpp"
#include "GS3DIndLightAlgo.hpp"
#include <fstream>

namespace GSGI
{

namespace
{
constexpr const char* kPersistKey = "GS3D";
constexpr const char* kVersion = ""; // __TIMESTAMP__; // Ignore version
} // namespace

using IndLightSplatPersist = SerializePersist<std::string, std::vector<GS3DIndLightSplat>>;
GSGI_SERIALIZER_REGISTER_POD(GS3DIndLightSplat);

void GS3DIndLightSplat::persistMesh(const ref<GMesh>& pMesh, std::span<const GS3DIndLightSplat> splats)
{
    auto meshSplatPersistPath = pMesh->getPersistPath(kPersistKey);
    IndLightSplatPersist::store(std::ofstream{meshSplatPersistPath}, kVersion, splats);
}

std::vector<GS3DIndLightSplat> GS3DIndLightSplat::fromMesh(const ref<GMesh>& pMesh, uint splatCount)
{
    std::vector<GS3DIndLightSplat> meshSplats;
    auto meshSplatPersistPath = pMesh->getPersistPath(kPersistKey);
    if (!IndLightSplatPersist::load(std::ifstream{meshSplatPersistPath}, kVersion, meshSplats) || meshSplats.size() != splatCount)
    {
        meshSplats = GS3DIndLightAlgo::getSplatsFromMeshFallback(pMesh, splatCount);
        IndLightSplatPersist::store(std::ofstream{meshSplatPersistPath}, kVersion, meshSplats);
    }
    return meshSplats;
}

} // namespace GSGI
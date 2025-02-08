//
// Created by adamyuan on 2/8/25.
//

#ifndef GSGI_MESHVHBVH_HPP
#define GSGI_MESHVHBVH_HPP

#include <Falcor.h>
#include "MeshBVH.hpp"

using namespace Falcor;

namespace GSGI {

struct MeshVHBVHBuilder
{
    struct PrimitiveInfo
    {
        float3 center;
        float3 halfExtent;
        AABB getAABB() const { return AABB{center - halfExtent, center + halfExtent}; }
    };
    std::vector<PrimitiveInfo> primitiveInfos;

    const AABB& root(const Concepts::MeshView auto& meshView)
    {
        uint32_t primitiveCount = meshView.getPrimitiveCount();
        primitiveInfos.resize(primitiveCount);
        for (uint32_t primitiveID = 0; primitiveID < primitiveCount; ++primitiveID)
        {
            auto [p0, p1, p2] = PrimitiveViewMethod::getVertexPositions(meshView.getPrimitive(primitiveID));
            auto primitiveAABB = AABB{p0, p0}.include(p1).include(p2);
            primitiveInfos[primitiveID] = {
                .center = primitiveAABB.center(),
                .halfExtent = primitiveAABB.extent() * 0.5f,
            };
        }
        return meshView.getAABB();
    }
    MeshBVHSplit<AABB> split(const Concepts::MeshView auto& meshView, std::span<uint32_t> primitiveIDs, const AABB& bound) const
    {
        auto boundExtent = bound.extent();
        // Use the axis with largest extent as split axis
        int splitAxis = boundExtent.z > boundExtent.x && boundExtent.z > boundExtent.y ? 2 : int(boundExtent.y > boundExtent.x);
        std::sort(
            primitiveIDs.begin(),
            primitiveIDs.end(),
            [&](uint32_t l, uint32_t r) { return primitiveInfos[l].center[splitAxis] < primitiveInfos[r].center[splitAxis]; }
        );
        // primitiveIDs.size() >= 2
        std::vector<AABB> rightBounds(primitiveIDs.size() - 1);

#define BOUND(POS) primitiveInfos[primitiveIDs[POS]].getAABB()
#define RIGHT_BOUND(SPLIT_POS) rightBounds[(SPLIT_POS) - 1] // SPLIT_POS >= 1 is guaranteed, so we can reduce the size of rightBounds vector

        RIGHT_BOUND(primitiveIDs.size() - 1) = BOUND(primitiveIDs.size() - 1);
        for (uint32_t i = primitiveIDs.size() - 2; i > 0; --i)
        {
            RIGHT_BOUND(i) = BOUND(i);
            RIGHT_BOUND(i).include(RIGHT_BOUND(i + 1));
        }

        static_assert(std::numeric_limits<double>::is_iec559);
        const auto getScore = [count = primitiveIDs.size()](uint32_t splitPos, const AABB& leftBound, const AABB& rightBound)
        {
            uint32_t leftCount = splitPos, rightCount = count - splitPos;
            // Volume heuristic (contrast to surface area heuristic)
            // The probability of entering a node is (approximately) in proportion to the volume of its bound
            return -(float(leftCount) * leftBound.volume() + float(rightCount) * rightBound.volume());
        };

        // Init as the first split
        AABB leftBound = BOUND(0);
        uint32_t optimalSplitPos = 1;
        float optimalScore = getScore(optimalSplitPos, leftBound, RIGHT_BOUND(1));
        AABB optimalLeftBound = leftBound;

        // Check other splits
        for (uint32_t splitPos = 2; splitPos < primitiveIDs.size(); ++splitPos)
        {
            leftBound.include(BOUND(splitPos - 1));
            float score = getScore(splitPos, leftBound, RIGHT_BOUND(splitPos));

            if (score > optimalScore)
            {
                optimalScore = score;
                optimalSplitPos = splitPos;
                optimalLeftBound = leftBound;
            }
        }

        return {
            .leftBound = optimalLeftBound,
            .rightBound = RIGHT_BOUND(optimalSplitPos),
            .splitPos = optimalSplitPos,
        };

#undef RIGHT_BOUND
#undef BOUND
    }
};

}

#endif // GSGI_MESHVHBVH_HPP

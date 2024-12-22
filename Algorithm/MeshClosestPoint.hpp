//
// Created by adamyuan on 12/22/24.
//

#ifndef GSGI_MESHCLOSESTPOINT_HPP
#define GSGI_MESHCLOSESTPOINT_HPP

#include <Falcor.h>
#include "MeshBVH.hpp"

using namespace Falcor;

namespace GSGI
{

using MeshClosestPointBVH = MeshBVH<AABB>;

struct MeshClosestPointBVHBuilder
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
            float3 p0 = meshView.getPrimitive(primitiveID).getVertex(0).getPosition();
            float3 p1 = meshView.getPrimitive(primitiveID).getVertex(1).getPosition();
            float3 p2 = meshView.getPrimitive(primitiveID).getVertex(2).getPosition();
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
        const auto getBoundDistance = [splitAxis](const AABB& leftBound, const AABB& rightBound)
        { return rightBound.minPoint[splitAxis] - leftBound.maxPoint[splitAxis]; };

        // Init as the first split
        AABB leftBound = BOUND(0);
        float optimalDistance = getBoundDistance(leftBound, RIGHT_BOUND(1));
        uint32_t optimalSplitPos = 1;
        AABB optimalLeftBound = leftBound;

        // Check other splits
        for (uint32_t splitPos = 2; splitPos < primitiveIDs.size(); ++splitPos)
        {
            leftBound.include(BOUND(splitPos - 1));
            float distance = getBoundDistance(leftBound, RIGHT_BOUND(splitPos));

            if (distance > optimalDistance)
            {
                optimalDistance = distance;
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

class MeshClosestPoint
{};

} // namespace GSGI

#endif // GSGI_MESHCLOSESTPOINT_HPP

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

struct MeshClosestPoint
{
    struct Result
    {
        std::optional<uint32_t> optPrimitiveID;
        float dist2{};
    };

    static MeshClosestPointBVH buildBVH(const Concepts::MeshView auto& meshView)
    {
        return MeshBVH<AABB>::build<MeshClosestPointBVHBuilder>(meshView);
    }

    static float getAABBDist2(const float3& point, const AABB& aabb)
    {
        float3 d = math::max(math::max(point - aabb.maxPoint, float3(0)), aabb.minPoint - point);
        return math::dot(d, d);
    }

    static float getPrimitiveDist2(const float3& point, const Concepts::PrimitiveView auto& primitiveView)
    {
        // From https://iquilezles.org/articles/triangledistance/
        auto [v1, v2, v3] = PrimitiveViewMethod::getVertexPositions(primitiveView);
        float3 v21 = v2 - v1;
        float3 p1 = point - v1;
        float3 v32 = v3 - v2;
        float3 p2 = point - v2;
        float3 v13 = v1 - v3;
        float3 p3 = point - v3;
        float3 nor = math::cross(v21, v13);

        const auto dot2 = [](const auto& x) { return math::dot(x, x); };

        return                                                         // inside/outside test
            (                                                          //
                math::sign(math::dot(math::cross(v21, nor), p1)) +     //
                    math::sign(math::dot(math::cross(v32, nor), p2)) + //
                    math::sign(math::dot(math::cross(v13, nor), p3)) <
                2.0
            )
                ?
                // 3 edges
                math::min(
                    math::min(
                        dot2(v21 * math::clamp(math::dot(v21, p1) / dot2(v21), 0.0f, 1.0f) - p1),
                        dot2(v32 * math::clamp(math::dot(v32, p2) / dot2(v32), 0.0f, 1.0f) - p2)
                    ),
                    dot2(v13 * math::clamp(math::dot(v13, p3) / dot2(v13), 0.0f, 1.0f) - p3)
                )
                :
                // 1 face
                math::dot(nor, p1) * math::dot(nor, p1) / dot2(nor);
    }

    static Result query(const Concepts::MeshView auto& meshView, const MeshClosestPointBVH& bvh, const float3& point, float maxDist2)
    {
        float dist2 = maxDist2;
        std::optional<uint32_t> optPrimitiveID = std::nullopt;

        const auto queryImpl = [&](const MeshClosestPointBVH::NodeView& node, float aabbDist2, auto&& queryFunc)
        {
            if (aabbDist2 >= dist2)
                return;

            if (node.isLeaf())
            {
                uint32_t primitiveID = node.getLeafPrimitiveID();
                float primitiveDist2 = getPrimitiveDist2(point, meshView.getPrimitive(primitiveID));
                if (primitiveDist2 < dist2)
                {
                    dist2 = primitiveDist2;
                    optPrimitiveID = primitiveID;
                }
            }
            else
            {
                const auto& leftChild = bvh.getNode(node.getLeftChildID());
                const auto& rightChild = bvh.getNode(node.getRightChildID());
                float leftAabbDist2 = getAABBDist2(point, leftChild.getBound());
                float rightAabbDist2 = getAABBDist2(point, rightChild.getBound());

                // Query child with smaller distance first
                if (leftAabbDist2 < rightAabbDist2)
                {
                    queryFunc(leftChild, leftAabbDist2, queryFunc);
                    queryFunc(rightChild, rightAabbDist2, queryFunc);
                }
                else
                {
                    queryFunc(rightChild, rightAabbDist2, queryFunc);
                    queryFunc(leftChild, leftAabbDist2, queryFunc);
                }
            }
        };

        const auto& rootNode = bvh.getRootNode();
        queryImpl(rootNode, getAABBDist2(point, rootNode.getBound()), queryImpl);

        return Result{
            .optPrimitiveID = optPrimitiveID,
            .dist2 = dist2,
        };
    }
};

} // namespace GSGI

#endif // GSGI_MESHCLOSESTPOINT_HPP

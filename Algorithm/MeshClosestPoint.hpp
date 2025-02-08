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

namespace Concepts
{

template<typename T, typename Bound_T>
concept MeshClosestPointFinder = requires(const float3& point, const Bound_T& bound) {
    { T::getDist2(point, bound) } -> std::convertible_to<float>;
};

} // namespace Concepts

struct MeshClosestPointAABBFinder
{
    static float getDist2(const float3& point, const AABB& aabb)
    {
        float3 d = math::max(math::max(point - aabb.maxPoint, float3(0)), aabb.minPoint - point);
        return math::dot(d, d);
    }
};

struct MeshClosestPoint
{
    struct Result
    {
        std::optional<uint32_t> optPrimitiveID;
        float dist2{};
    };

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

    template<typename Bound_T, Concepts::MeshClosestPointFinder<Bound_T> Finder_T>
    static Result query(const Concepts::MeshView auto& meshView, const MeshBVH<Bound_T>& bvh, const float3& point, float maxDist2)
    {
        float dist2 = maxDist2;
        std::optional<uint32_t> optPrimitiveID = std::nullopt;

        const auto queryImpl = [&](const typename MeshBVH<Bound_T>::NodeView& node, float aabbDist2, auto&& queryFunc)
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
                float leftAabbDist2 = Finder_T::getDist2(point, leftChild.getBound());
                float rightAabbDist2 = Finder_T::getDist2(point, rightChild.getBound());

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
        queryImpl(rootNode, Finder_T::getDist2(point, rootNode.getBound()), queryImpl);

        return Result{
            .optPrimitiveID = optPrimitiveID,
            .dist2 = dist2,
        };
    }
};

} // namespace GSGI

#endif // GSGI_MESHCLOSESTPOINT_HPP

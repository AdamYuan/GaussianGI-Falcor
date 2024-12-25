//
// Created by adamyuan on 12/17/24.
//

#ifndef GSGI_MESHVIEW_HPP
#define GSGI_MESHVIEW_HPP

#include <concepts>
#include <Falcor.h>
#include <Utils/Math/AABB.h>

using namespace Falcor;

namespace GSGI
{

namespace Concepts
{

template<typename T>
concept VertexView = requires(const T ct) {
    { ct.getPosition() } -> std::convertible_to<float3>;
    // std::is_pod_v<decltype(ct.info)>;
};

template<typename T>
concept PrimitiveView = requires(const T ct) {
    ct.getVertex(0);
    requires VertexView<std::remove_cvref_t<decltype(ct.getVertex(0))>>;
    // std::is_pod_v<decltype(ct.info)>;
};

template<typename T>
concept MeshView = requires(const T ct) {
    ct.getPrimitive(0);
    { ct.getPrimitiveCount() } -> std::convertible_to<std::size_t>;
    requires PrimitiveView<std::remove_cvref_t<decltype(ct.getPrimitive(0))>>;
    { ct.getAABB() } -> std::convertible_to<AABB>;
};

} // namespace Concepts

struct PrimitiveViewMethod
{
    static std::tuple<float3, float3, float3> getVertexPositions(const Concepts::PrimitiveView auto& primitiveView)
    {
        float3 p0 = primitiveView.getVertex(0).getPosition();
        float3 p1 = primitiveView.getVertex(1).getPosition();
        float3 p2 = primitiveView.getVertex(2).getPosition();
        return {p0, p1, p2};
    }
    static float3 getGeomNormal(const Concepts::PrimitiveView auto& primitiveView)
    {
        auto [p0, p1, p2] = getVertexPositions(primitiveView);
        return math::normalize(math::cross(p1 - p0, p2 - p0));
    }
};

struct MeshPoint
{
    uint32_t primitiveID;
    float2 barycentrics;

    float3 getPosition(const Concepts::MeshView auto& meshView) const
    {
        auto [p0, p1, p2] = PrimitiveViewMethod::getVertexPositions(meshView.getPrimitive(primitiveID));
        return p0 * (1.0f - barycentrics.x - barycentrics.y) + p1 * barycentrics.x + p2 * barycentrics.y;
    }
};

static_assert(sizeof(MeshPoint) == 3 * sizeof(uint32_t));

} // namespace GSGI

#endif // GSGI_MESHVIEW_HPP

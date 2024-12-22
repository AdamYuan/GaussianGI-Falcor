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

} // namespace GSGI

#endif // GSGI_MESHVIEW_HPP

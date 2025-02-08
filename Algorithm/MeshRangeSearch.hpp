//
// Created by adamyuan on 2/8/25.
//

#ifndef GSGI_MESHSEARCH_HPP
#define GSGI_MESHSEARCH_HPP

#include <Falcor.h>
#include "MeshBVH.hpp"

using namespace Falcor;

namespace GSGI
{

namespace Concepts
{
template<typename T, typename Bound_T, typename Primitive_T>
concept MeshRangeSearcher = requires(const T& ct, const Bound_T& bound, const Primitive_T& primitive) {
    { ct.isIntersected(bound) } -> std::convertible_to<bool>;
    { ct.isIntersected(primitive) } -> std::convertible_to<bool>;
};
} // namespace Concepts

struct MeshRangeSearch
{
    struct Result
    {
        std::vector<uint32_t> primitiveIDs;
    };

    template<
        Concepts::MeshView MeshView_T,
        typename Bound_T,
        Concepts::MeshRangeSearcher<Bound_T, MeshViewMethod::PrimitiveView<MeshView_T>> Searcher_T>
    static Result query(const MeshView_T& meshView, const MeshBVH<Bound_T>& bvh, const Searcher_T& searcher)
    {
        std::vector<uint32_t> primitiveIDs;

        const auto queryImpl = [&](const typename MeshBVH<Bound_T>::NodeView& node, auto&& queryFunc)
        {
            if (node.isLeaf())
            {
                uint32_t primitiveID = node.getLeafPrimitiveID();
                if (searcher.isIntersected(meshView.getPrimitive(primitiveID)))
                    primitiveIDs.push_back(primitiveID);
            }
            else
            {
                const auto& leftChild = bvh.getNode(node.getLeftChildID());
                const auto& rightChild = bvh.getNode(node.getRightChildID());

                if (searcher.isIntersected(leftChild.getBound()))
                    queryFunc(leftChild, queryFunc);

                if (searcher.isIntersected(rightChild.getBound()))
                    queryFunc(rightChild, queryFunc);
            }
        };

        const auto& rootNode = bvh.getRootNode();
        if (searcher.isIntersected(rootNode.getBound()))
            queryImpl(rootNode, queryImpl);

        return Result{
            .primitiveIDs = std::move(primitiveIDs),
        };
    }
};

} // namespace GSGI

#endif // GSGI_MESHSEARCH_HPP

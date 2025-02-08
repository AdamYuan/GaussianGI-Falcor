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
template<typename T, typename Range_T, typename Bound_T, typename Primitive_T>
concept MeshRangeSearcher = requires(const Range_T& range, const Bound_T& bound, const Primitive_T& primitive) {
    T::isIntersected(range, bound);
    T::isIntersected(range, primitive);
};
} // namespace Concepts

struct MeshRangeSearch
{
    struct Result
    {
        std::vector<uint32_t> primitiveIDs;
    };

    template<typename Searcher_T, typename Range_T, typename Bound_T, Concepts::MeshView MeshView_T>
    static Result query(const MeshView_T& meshView, const MeshBVH<Bound_T>& bvh, const Range_T& range)
        requires Concepts::MeshRangeSearcher<Searcher_T, Range_T, Bound_T, MeshViewMethod::PrimitiveView<MeshView_T>>
    {
        std::vector<uint32_t> primitiveIDs;

        const auto queryImpl = [&](const typename MeshBVH<Bound_T>::NodeView& node, auto&& queryFunc)
        {
            if (node.isLeaf())
            {
                uint32_t primitiveID = node.getLeafPrimitiveID();
                if (Searcher_T::isIntersected(range, meshView.getPrimitive(primitiveID)))
                    primitiveIDs.push_back(primitiveID);
            }
            else
            {
                const auto& leftChild = bvh.getNode(node.getLeftChildID());
                const auto& rightChild = bvh.getNode(node.getRightChildID());

                if (Searcher_T::isIntersected(range, leftChild.getBound()))
                    queryFunc(leftChild, queryFunc);

                if (Searcher_T::isIntersected(range, rightChild.getBound()))
                    queryFunc(rightChild, queryFunc);
            }
        };

        const auto& rootNode = bvh.getRootNode();
        if (Searcher_T::isIntersected(range, rootNode.getBound()))
            queryImpl(rootNode, queryImpl);

        return Result{
            .primitiveIDs = std::move(primitiveIDs),
        };
    }
};

} // namespace GSGI

#endif // GSGI_MESHSEARCH_HPP

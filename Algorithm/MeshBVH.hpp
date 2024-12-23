//
// Created by adamyuan on 12/22/24.
//

#ifndef GSGI_MESHBVH_HPP
#define GSGI_MESHBVH_HPP

#include <Falcor.h>
#include <concepts>
#include "MeshView.hpp"

using namespace Falcor;

namespace GSGI
{

template<typename Bound_T>
struct MeshBVHSplit
{
    Bound_T leftBound, rightBound;
    uint32_t splitPos;
};

namespace Concepts
{

template<typename T, typename Bound_T>
concept MeshBVHBuilder = requires(T& t, const MeshView auto& meshView) {
    t = T{};
    { t.root(meshView) } -> std::convertible_to<Bound_T>;
    { t.split(meshView, std::span<uint32_t>{}, Bound_T{}) } -> std::convertible_to<MeshBVHSplit<Bound_T>>;
};

} // namespace Concepts

template<typename Bound_T>
class MeshBVH
{
private:
    struct Node
    {
        static constexpr uint32_t kLeafFlag = 1 << 31;
        Bound_T bound{};
        uint32_t data{};

        static Node makeInner(const Bound_T& bound, uint32_t rightChildID)
        {
            FALCOR_ASSERT((rightChild & kLeafFlag) == 0);
            return {.bound = bound, .data = rightChildID};
        }
        static Node makeLeaf(const Bound_T& bound, uint32_t primitiveID)
        {
            FALCOR_ASSERT((primitiveID & kLeafFlag) == 0);
            return {.bound = bound, .data = primitiveID | kLeafFlag};
        }
    };
    std::vector<Node> mNodes;

public:
    enum class NodeID : uint32_t
    {

    };

    class NodeView
    {
    private:
        const Node& mNode;
        NodeID mNodeID{};

    public:
        NodeView(const Node& node, NodeID nodeID) : mNode{node}, mNodeID{nodeID} {}

        const Bound_T& getBound() const { return mNode.bound; }

        bool isLeaf() const { return mNode.data & Node::kLeafFlag; }
        uint32_t getLeafPrimitiveID() const
        {
            FALCOR_ASSERT(isLeaf());
            return mNode.data & (~Node::kLeafFlag);
        }
        bool isInner() const { return !isLeaf(); }
        NodeID getLeftChildID() const
        {
            FALCOR_ASSERT(isInner());
            return static_cast<NodeID>(static_cast<uint32_t>(mNodeID) + 1);
        }
        NodeID getRightChildID() const
        {
            FALCOR_ASSERT(isInner());
            return static_cast<NodeID>(mNode.data);
        }
    };

    static NodeID getRootID() { return static_cast<NodeID>(0); }
    const NodeView& getNode(NodeID nodeID) const { return NodeView(mNodes[static_cast<uint32_t>(nodeID)], nodeID); }
    const NodeView& getRootNode() const { return getNode(getRootID()); }

    template<Concepts::MeshBVHBuilder<Bound_T> Builder_T, Concepts::MeshView MeshView_T>
    static MeshBVH build(const MeshView_T& meshView)
    {
        uint32_t primitiveCount = meshView.getPrimitiveCount();
        uint32_t nodeCount = primitiveCount << 1 | 1;

        FALCOR_CHECK(primitiveCount > 0, "");

        MeshBVH bvh = {};
        bvh.mNodes.reserve(nodeCount); // Important

        std::vector<uint32_t> primitiveIDs(primitiveCount);
        for (uint32_t i = 0; i < primitiveCount; ++i)
            primitiveIDs[i] = i;

        Builder_T builder = {};

        const auto buildImpl =
            [&meshView, &builder, &bvh](std::span<uint32_t> primitiveIDs, const Bound_T& bound, auto&& buildFunc) -> uint32_t
        {
            FALCOR_CHECK(!primitiveIDs.empty(), "");

            uint32_t nodeID = bvh.mNodes.size();
            bvh.mNodes.emplace_back();
            auto& node = bvh.mNodes.back(); // Since we have reserved space in std::vector, the reference will remain valid

            if (primitiveIDs.size() == 1)
                node = Node::makeLeaf(bound, primitiveIDs[0]);
            else
            {
                MeshBVHSplit<Bound_T> split = builder.split(meshView, primitiveIDs, bound);
                buildFunc(primitiveIDs.subspan(0, split.splitPos), split.leftBound, buildFunc);
                uint rightChildID = buildFunc(primitiveIDs.subspan(split.splitPos), split.rightBound, buildFunc);
                node = Node::makeInner(bound, rightChildID);
            }
            return nodeID;
        };

        Bound_T bound = builder.root(meshView);
        buildImpl(primitiveIDs, bound, buildImpl);

        return bvh;
    }
};

} // namespace GSGI

#endif // GSGI_MESHBVH_HPP

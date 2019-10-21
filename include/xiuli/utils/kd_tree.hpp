#pragma once
#include <limits>       // std::numeric_limits
#include <iostream>
#include "xtensor/xtensor.hpp"
#include "xtensor-python/pytensor.hpp"
#include "xtensor/xview.hpp"
#include "xtensor/xnorm.hpp"
#include "xtensor/xsort.hpp"
#include "xtensor/xindex_view.hpp"

// use the c++17 nested namespace
namespace xiuli::utils{

using NodesType = xt::xtensor<float, 2>;

auto next_dim(std::size_t &dim) {
    if(dim==3)
        dim = 0;
    else
        dim += 1;
    return dim;
}

// ThreeDTree
class ThreeDNode{
public:
    // we need to make base class polymorphic for dynamic cast
    virtual std::size_t size() const = 0;
    
    virtual xt::xtensor<std::size_t, 1> get_node_indices() const = 0;
    
    virtual void fill_node_indices(xt::xtensor<std::size_t, 1> &nodeIndicesBuffer, 
                                    std::size_t &startIndex) const = 0;
    
    virtual xt::xtensor<std::size_t, 1> find_nearest_k_node_indices(
                const xt::xtensor<float, 1> &queryNode,
                const NodesType &nodes, 
                std::size_t &dim, const std::size_t &nearestNodeNum) const = 0; 
};
using ThreeDNodePtr = std::shared_ptr<ThreeDNode>;


class ThreeDLeafNode: public ThreeDNode{

private:
    xt::xtensor<std::size_t, 1> nodeIndices;

public:
    ThreeDLeafNode() = default;
    ThreeDLeafNode(const xt::xtensor<std::size_t, 1> &nodeIndices_) : nodeIndices(nodeIndices_){}

    std::size_t size() const {
        return nodeIndices.size();
    }

    xt::xtensor<std::size_t, 1> get_node_indices() const {
        return nodeIndices;
    }

    void fill_node_indices(xt::xtensor<std::size_t, 1> &nodeIndicesBuffer, 
                                        std::size_t &startIndex) const {
        for (std::size_t i=0; i<nodeIndices.size(); i++){
            nodeIndicesBuffer( i + startIndex ) = nodeIndices(i);
        }
    }

    xt::xtensor<std::size_t, 1> find_nearest_k_node_indices( 
                const xt::xtensor<float, 1> &queryNode, 
                const NodesType &nodes,
                std::size_t &dim, const std::size_t &nearestNodeNum) const {
        
        if (nearestNodeNum == nodeIndices.size()){
            return nodeIndices;
        } else if (nearestNodeNum == 1){
            // use squared distance to avoid sqrt computation
            float minSquaredDist = std::numeric_limits<float>::max();
            std::size_t nearestNodeIndex = 0;
            for (auto nodeIndex : nodeIndices){
                auto node = xt::view(nodes, nodeIndex, xt::range(0, 3));
                float squaredDist = xt::norm_sq(node - queryNode)(0);
                if (squaredDist < minSquaredDist){
                    nearestNodeIndex = nodeIndex;
                    minSquaredDist = squaredDist;
                }
            }
            return {nearestNodeIndex};
        } else {
            assert(nearestNodeNum < nodeIndices.size());
            xt::xtensor<float, 1>::shape_type sh = {nearestNodeNum};
            xt::xtensor<float, 1> dist2s = xt::empty<float>(sh); 
            for (std::size_t i=0; i<nodeIndices.size(); i++){
                auto nodeIndex = nodeIndices( i );
                auto node = xt::view(nodes, nodeIndex, xt::range(0, 3));
                dist2s(i) = xt::norm_sq( node - queryNode )(0);
            }
            auto indices = xt::argpartition( dist2s, nearestNodeNum-1 );
            auto selectedIndices = xt::view(indices, xt::range(0, nearestNodeNum));
            return xt::index_view(nodeIndices, selectedIndices );
        }
    }
};
using ThreeDLeafNodePtr = std::shared_ptr<ThreeDLeafNode>;

class ThreeDInsideNode: public ThreeDNode{
public: 
    std::size_t medianNodeIndex;
    ThreeDNodePtr leftNodePtr;
    ThreeDNodePtr rightNodePtr;
    std::size_t nodeNum;

    ThreeDInsideNode() = default;

    ThreeDInsideNode(const std::size_t &medianNodeIndex_, 
            ThreeDNodePtr leftNodePtr_, ThreeDNodePtr rightNodePtr_, std::size_t nodeNum_):
            medianNodeIndex(medianNodeIndex_), leftNodePtr(leftNodePtr_), 
            rightNodePtr(rightNodePtr_), nodeNum(nodeNum_){};

    std::size_t size() const {
        // return leftNodePtr->size() + rightNodePtr->size() + 1;
        return nodeNum;
    }
     
    xt::xtensor<std::size_t, 1> get_node_indices() const {
        xt::xtensor<std::size_t, 1>::shape_type sh = {nodeNum};
        xt::xtensor<std::size_t, 1> nodeIndicesBuffer =  xt::empty<std::size_t>(sh);

        std::size_t filledNum = 0;
        fill_node_indices( nodeIndicesBuffer, filledNum );
        return nodeIndicesBuffer;
    }
    
    void fill_node_indices( xt::xtensor<std::size_t, 1> &nodeIndicesBuffer, 
                                            std::size_t &startIndex) const {
        leftNodePtr->fill_node_indices(nodeIndicesBuffer, startIndex);
        startIndex += leftNodePtr->size();
        nodeIndicesBuffer(startIndex) = medianNodeIndex;
        startIndex += 1;
        rightNodePtr->fill_node_indices( nodeIndicesBuffer, startIndex );
        startIndex += rightNodePtr->size();
    }
   
    xt::xtensor<std::size_t, 1> find_nearest_k_node_indices( 
                const xt::xtensor<float, 1> &queryNode,
                const xt::xtensor<float, 2> &nodes, 
                std::size_t &dim, const std::size_t &nearestNodeNum) const {
        ThreeDNodePtr closeNodePtr, farNodePtr;
        
        // compare with the median value
        if (queryNode(dim) < nodes(medianNodeIndex, dim)){
            // continue checking left nodes
            closeNodePtr = leftNodePtr;
            farNodePtr = rightNodePtr;
        } else {
            closeNodePtr = rightNodePtr;
            farNodePtr = leftNodePtr;
        }

        if (closeNodePtr->size() >= nearestNodeNum){
            dim = next_dim(dim);
            return closeNodePtr->find_nearest_k_node_indices(
                                        queryNode, nodes, dim, nearestNodeNum);
        } else {
            // closeNodePtr->size() < nearestNodeNum
            // we need to add some close nodes from far node
            xt::xtensor<std::size_t, 1>::shape_type shape = {nearestNodeNum};
            xt::xtensor<std::size_t, 1> nearestNodeIndices = xt::empty<std::size_t>(shape);
            auto closeNodeIndices = closeNodePtr->get_node_indices();
            xt::view(nearestNodeIndices, xt::range(0, closeNodeIndices.size())) = closeNodeIndices;
            //for (std::size_t i = 0; i<closeNodeIndices.size(); i++){
                //nearestNodeIndices(i) = closeNodeIndices(i);
            //}
            auto remainingNodeNum = nearestNodeNum - closeNodeIndices.size();
            auto remainingNodeIndices = farNodePtr->find_nearest_k_node_indices(
                                            queryNode, nodes, dim, remainingNodeNum);
            xt::view(nearestNodeIndices, xt::range(closeNodeIndices.size(), _)) = remainingNodeIndices;
            return nearestNodeIndices;
        }
    }
}; // ThreeDNode class

using ThreeDInsideNodePtr = std::shared_ptr<ThreeDInsideNode>;

class ThreeDTree{
private:
    ThreeDInsideNodePtr root;
    const NodesType nodes;
    const std::size_t leafSize;

    template<class E>
    ThreeDInsideNodePtr build_node(const E &nodeIndices, std::size_t &dim){
        // find the median value index
        xt::xtensor<std::size_t, 1> coords = xt::index_view( 
                            xt::view(nodes, xt::all(), dim), 
                            nodeIndices);
        const std::size_t medianIndex = nodeIndices.size() / 2;
        const auto partitionIndices = xt::argpartition(coords, medianIndex);
        const auto middleNodeIndex = partitionIndices(medianIndex);
        const xt::xtensor<std::size_t, 1> partitionedNodeIndices = xt::index_view( 
                                                        nodeIndices, partitionIndices );
        const auto leftNodeIndices = xt::view(partitionedNodeIndices, xt::range(0, medianIndex));
        const auto rightNodeIndices = xt::view(
                                partitionedNodeIndices, xt::range(medianIndex, _));

        ThreeDNodePtr leftNodePtr, rightNodePtr;
        // recursively loop the dimension in 3D
        dim = next_dim(dim);       
        if (medianIndex > leafSize){
            leftNodePtr = build_node( leftNodeIndices, dim );
            rightNodePtr = build_node( rightNodeIndices, dim );
        } else {
            // include all the nodes as a leaf
            leftNodePtr = std::static_pointer_cast<ThreeDNode>(
                            std::make_shared<ThreeDLeafNode>( leftNodeIndices ));
            rightNodePtr = std::static_pointer_cast<ThreeDNode>( 
                            std::make_shared<ThreeDLeafNode>( rightNodeIndices ));
        }

        return std::make_shared<ThreeDInsideNode>(middleNodeIndex, 
                        leftNodePtr, rightNodePtr, coords.size());
    }

    auto build_root(){
        // start from first dimension
        std::size_t dim = 0;
        auto nodeIndices = xt::arange<std::size_t>(0, nodes.shape(0));
        root = build_node( nodeIndices, dim );
    }

public:
    ThreeDTree() = default;

    ThreeDTree(const NodesType &nodes_, const std::size_t leafSize_=10): 
            nodes(nodes_), leafSize(leafSize_){
        build_root();
    }

    ThreeDTree(const xt::pytensor<float, 2> &nodes_, const std::size_t leafSize_=10):
        nodes( NodesType(nodes_) ), leafSize(leafSize_){
        build_root();
    }
 
    auto get_leaf_size() const {
        return leafSize;
    }
    
    xt::xtensor<std::size_t, 1> find_nearest_k_node_indices(
                const xt::xtensor<float, 1> &queryNode, 
                const std::size_t &nearestNodeNum) const {
        assert( nearestNodeNum >= 1 );
        std::size_t dim = 0;
        auto queryNodeCoord = xt::view(queryNode, xt::range(0, 3));
        return root->find_nearest_k_node_indices(queryNodeCoord, nodes, dim, nearestNodeNum);
    }

    xt::xtensor<std::size_t, 1> find_nearest_k_node_indices(
            const xt::pytensor<float, 1> &queryNode, 
            const std::size_t &nearestNodeNum){
        return find_nearest_k_node_indices( xt::xtensor<float,1>(queryNode), nearestNodeNum );
    }

    auto find_nearest_k_nodes(const xt::xtensor<float, 1> &queryNode, 
                                        const std::size_t &nearestNodeNum) const {
        
        auto nearestNodeIndices = find_nearest_k_node_indices(queryNode, nearestNodeNum);
        xt::xtensor<float, 2>::shape_type shape = {nearestNodeNum, 4};

        auto nearestNodes = xt::empty<float>( shape ); 
        for(std::size_t i; i<nearestNodeNum; i++){
            auto nodeIndex = nearestNodeIndices(i);
            xt::view(nearestNodes, i, xt::all()) = xt::view(nodes, nodeIndex, xt::all());
        }
        return nearestNodes;
    } 
}; // ThreeDTree class

} // end of namespace
#pragma once

#include <fstream>
#include <iostream>
#include <limits>       // std::numeric_limits
#include <filesystem>
#include "xtensor/xtensor.hpp"
#include "xtensor/xfixed.hpp"
#include "xtensor/xview.hpp"
#include "xtensor-python/pytensor.hpp"     // Numpy bindings
#include "xtensor/xcsv.hpp"


using namespace xt::placeholders;  // required for `_` to work

// use the c++17 nested namespace
namespace xiuli::neuron::nblast{

using NodesType = xt::xtensor<float, 2>;
using TableType = xt::xtensor_fixed<float, xt::xshape<21, 10>>; 
using DistThresholdsType = xt::xtensor_fixed<float, xt::xshape<22>>; 
using ADPThresholdsType = xt::xtensor_fixed<float, xt::xshape<11>>; 


class ScoreTable{

private:
    TableType table;
    const DistThresholdsType distThresholds = {0., 0.75, 1.5, 2, 2.5, 3, 3.5, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 20, 25, 30, 40, std::numeric_limits<float>::max()};
    const ADPThresholdsType adpThresholds = {0., 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};

    template<std::size_t N>
    auto binary_search( const xt::xtensor_fixed<float, xt::xshape<N>> &thresholds, const float &value ){
        std::size_t start = 0;
        // Note that the last one index is N-1 rather than N_
        // This is following python indexing style, the last index is uninclusive
        std::size_t stop = N;
        while( stop - start > 1 ){
            auto middle = std::div(stop - start, 2).quot + start;
            if ( value > thresholds[ middle ] ){
                // Move forward
                start = middle;
            }else{
                // Move backward
                stop = middle;
            }
        }
        // the start is the matching index
        return start;
    }

public:
    ScoreTable(const xt::pytensor<float, 2> &table_): table(table_){}

    ScoreTable( const std::string fileName = std::filesystem::current_path() / "../../../data/smat_fcwb.csv"){
        std::ifstream in(fileName); 
        table = xt::load_csv<float>( in );
    }

    inline auto get_pytable(){
        // xtensor_fixed can not be converted to pytensor directly
        return xt::xtensor<float, 2>(table);
    }

    /**
     * \brief 
     * \param dist: physical distance
     * \param dp: absolute dot product of vectors
     */
    inline auto operator()(const float &dist, const float &adp){
        std::size_t distIdx = binary_search( distThresholds, dist );
        std::size_t adpIdx = binary_search( adpThresholds, adp );
        return table( distIdx, adpIdx );
    }

    inline auto operator()(const std::tuple<float, float> &slice){
        return this->operator()(std::get<0>(slice), std::get<1>(slice));
    }
};

} // end of namespace xiuli::neuron::nblast
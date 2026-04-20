#ifndef PAGING_SIMULATOR_HPP
#define PAGING_SIMULATOR_HPP

#include <string>
#include <vector>

#include "models.hpp"

class PagingSimulator {
public:
    PagingResult simulate(
        PagingAlgorithm algorithm,
        int frame_count,
        const std::vector<int>& references
    );

    static std::string algorithm_name(PagingAlgorithm algorithm);
};

#endif

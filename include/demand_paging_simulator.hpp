#ifndef DEMAND_PAGING_SIMULATOR_HPP
#define DEMAND_PAGING_SIMULATOR_HPP

#include <vector>

#include "models.hpp"

class DemandPagingSimulator {
public:
    DemandPagingResult simulate(
        int frame_count,
        int tlb_size,
        const std::vector<int>& references
    );
};

#endif

#ifndef MULTI_LEVEL_PAGING_SIMULATOR_HPP
#define MULTI_LEVEL_PAGING_SIMULATOR_HPP

#include <vector>

#include "models.hpp"

class MultiLevelPagingSimulator {
public:
    MultiLevelPagingResult simulate(
        int page_size,
        int inner_bits,
        int logical_address,
        const std::vector<MultiLevelEntry>& page_map
    );
};

#endif

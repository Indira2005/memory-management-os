#ifndef THRASHING_SIMULATOR_HPP
#define THRASHING_SIMULATOR_HPP

#include <vector>

#include "models.hpp"

class ThrashingSimulator {
public:
    ThrashingResult simulate(
        PagingAlgorithm algorithm,
        int min_frames,
        int max_frames,
        int thrashing_threshold,
        const std::vector<int>& references
    );
};

#endif

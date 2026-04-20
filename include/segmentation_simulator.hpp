#ifndef SEGMENTATION_SIMULATOR_HPP
#define SEGMENTATION_SIMULATOR_HPP

#include <vector>

#include "models.hpp"

class SegmentationSimulator {
public:
    SegmentationResult simulate(
        int logical_address,
        int bits_for_offset,
        const std::vector<SegmentEntry>& segment_table
    );
};

#endif

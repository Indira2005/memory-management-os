#include "segmentation_simulator.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

SegmentationResult SegmentationSimulator::simulate(
    int logical_address,
    int bits_for_offset,
    const std::vector<SegmentEntry>& segment_table
) {
    if (logical_address < 0) {
        throw std::runtime_error("Logical address must not be negative.");
    }
    if (bits_for_offset <= 0 || bits_for_offset >= 31) {
        throw std::runtime_error("Bits for offset must be between 1 and 30.");
    }
    if (segment_table.empty()) {
        throw std::runtime_error("Segment table must contain at least one entry.");
    }

    SegmentationResult result;
    result.logical_address = logical_address;
    result.bits_for_offset = bits_for_offset;
    result.segment_number = logical_address >> bits_for_offset;
    result.offset = logical_address & ((1 << bits_for_offset) - 1);
    result.segment_table = segment_table;

    const auto entry_it = std::find_if(
        segment_table.begin(),
        segment_table.end(),
        [&](const SegmentEntry& entry) { return entry.segment == result.segment_number; }
    );

    if (entry_it == segment_table.end()) {
        result.details = "No segment-table entry exists for the extracted segment number.";
        return result;
    }

    if (result.offset >= entry_it->limit) {
        std::ostringstream details;
        details << "Offset " << result.offset << " exceeds the segment limit of " << entry_it->limit
                << ", so a protection fault would occur.";
        result.details = details.str();
        return result;
    }

    result.valid = true;
    result.physical_address = entry_it->base + result.offset;

    std::ostringstream details;
    details << "Logical address " << logical_address << " maps to segment " << result.segment_number
            << " with offset " << result.offset << ". Using base " << entry_it->base
            << ", the physical address becomes " << result.physical_address << ".";
    result.details = details.str();
    return result;
}
#include "multi_level_paging_simulator.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

MultiLevelPagingResult MultiLevelPagingSimulator::simulate(
    int page_size,
    int inner_bits,
    int logical_address,
    const std::vector<MultiLevelEntry>& page_map
) {
    if (page_size <= 0) {
        throw std::runtime_error("Page size must be greater than 0.");
    }
    if (inner_bits <= 0 || inner_bits >= 31) {
        throw std::runtime_error("Inner-level bits must be between 1 and 30.");
    }
    if (logical_address < 0) {
        throw std::runtime_error("Logical address must not be negative.");
    }
    if (page_map.empty()) {
        throw std::runtime_error("Two-level page map must contain at least one entry.");
    }

    MultiLevelPagingResult result;
    result.page_size = page_size;
    result.logical_address = logical_address;
    result.inner_bits = inner_bits;
    result.page_number = logical_address / page_size;
    result.offset = logical_address % page_size;
    result.outer_index = result.page_number >> inner_bits;
    result.inner_index = result.page_number & ((1 << inner_bits) - 1);
    result.page_map = page_map;

    const auto entry_it = std::find_if(
        page_map.begin(),
        page_map.end(),
        [&](const MultiLevelEntry& entry) {
            return entry.outer_index == result.outer_index && entry.inner_index == result.inner_index;
        }
    );

    if (entry_it == page_map.end()) {
        result.details = "No second-level page-table entry was found for the derived outer and inner indexes.";
        return result;
    }

    if (!entry_it->present || entry_it->frame < 0) {
        result.details = "The second-level page-table entry exists, but the page is not currently present in memory.";
        return result;
    }

    result.found = true;
    result.frame_number = entry_it->frame;
    result.physical_address = entry_it->frame * page_size + result.offset;

    std::ostringstream details;
    details << "Logical address " << logical_address
            << " produced page number " << result.page_number
            << ", which splits into outer index " << result.outer_index
            << " and inner index " << result.inner_index
            << ". The frame is " << entry_it->frame
            << ", so the physical address is " << result.physical_address << ".";
    result.details = details.str();
    return result;
}

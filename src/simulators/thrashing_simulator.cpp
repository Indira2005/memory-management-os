#include "thrashing_simulator.hpp"

#include <sstream>
#include <stdexcept>

#include "paging_simulator.hpp"

ThrashingResult ThrashingSimulator::simulate(
    PagingAlgorithm algorithm,
    int min_frames,
    int max_frames,
    int thrashing_threshold,
    const std::vector<int>& references
) {
    if (min_frames <= 0 || max_frames <= 0 || min_frames > max_frames) {
        throw std::runtime_error("Frame range is invalid.");
    }
    if (thrashing_threshold < 0 || thrashing_threshold > 100) {
        throw std::runtime_error("Thrashing threshold must be between 0 and 100.");
    }
    if (references.empty()) {
        throw std::runtime_error("Reference string must not be empty.");
    }

    ThrashingResult result;
    result.algorithm_name = PagingSimulator::algorithm_name(algorithm);
    result.reference_string = references;
    result.thrashing_threshold = thrashing_threshold;

    PagingSimulator paging_simulator;
    for (int frames = min_frames; frames <= max_frames; ++frames) {
        const PagingResult paging = paging_simulator.simulate(algorithm, frames, references);
        const double fault_rate = static_cast<double>(paging.faults) * 100.0 / static_cast<double>(references.size());
        result.points.push_back({frames, paging.faults, fault_rate});
        if (fault_rate >= static_cast<double>(thrashing_threshold)) {
            result.detected = true;
        }
    }

    std::ostringstream details;
    if (result.detected) {
        details << "Thrashing-like behavior was detected because the page-fault rate crossed the threshold of "
                << thrashing_threshold << "% for at least one frame count.";
    } else {
        details << "The tested frame counts stayed below the chosen thrashing threshold of "
                << thrashing_threshold << "%.";
    }
    result.details = details.str();
    return result;
}

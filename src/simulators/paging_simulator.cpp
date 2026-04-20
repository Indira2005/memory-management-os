#include "paging_simulator.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
int find_page(const std::vector<int>& frames, int page) {
    for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
        //return 1 if page exists
        if (frames[i] == page) {
            return i;
        }
    }
    return -1;
}
}  // namespace

PagingResult PagingSimulator::simulate(
    PagingAlgorithm algorithm,
    int frame_count,
    const std::vector<int>& references
) {
    //check for frame count and ref string
    if (frame_count <= 0) {
        throw std::runtime_error("Frame count must be greater than 0.");
    }
    if (references.empty()) {
        throw std::runtime_error("Reference string must not be empty.");
    }

    PagingResult result;
    result.algorithm_name = algorithm_name(algorithm);
    result.frame_count = frame_count;
    result.reference_string = references;

    //initialise frames -> -1 == empty 
    std::vector<int> frames(frame_count, -1);
    std::vector<int> last_used(frame_count, -1);
    std::vector<int> reference_bits(frame_count, 0);
    int fifo_pointer = 0;
    int second_chance_pointer = 0;

    for (int step_index = 0; step_index < static_cast<int>(references.size()); ++step_index) {
        const int page = references[step_index];
        const int frame_index = find_page(frames, page);
        PagingStep step;
        step.reference = page;
        step.frames = frames;
        step.reference_bits = reference_bits;

        //if page found hit++
        if (frame_index >= 0) {
            step.hit = true;
            result.hits++;
            //update last used for lru and ref bit for sc 
            last_used[frame_index] = step_index;
            reference_bits[frame_index] = 1;

            std::ostringstream details;
            details << "Reference " << page << " is already in frame " << frame_index << ".";
            step.details = details.str();
            step.frames = frames;
            step.reference_bits = reference_bits;
            result.steps.push_back(step);
            continue;
        }

        step.page_fault = true;
        result.faults++;

        int target_frame = -1;
        //use empty frame if exists
        for (int i = 0; i < frame_count; ++i) {
            if (frames[i] == -1) {
                target_frame = i;
                break;
            }
        }

        //if no empty frame, apply algo
        if (target_frame == -1) {
            switch (algorithm) {
            case PagingAlgorithm::FIFO:
                //replace oldest and move pointer circularly 
                target_frame = fifo_pointer;
                fifo_pointer = (fifo_pointer + 1) % frame_count;
                break;

            case PagingAlgorithm::LRU: {
                //find min last used
                int oldest = std::numeric_limits<int>::max();
                for (int i = 0; i < frame_count; ++i) {
                    if (last_used[i] < oldest) {
                        oldest = last_used[i];
                        target_frame = i;
                    }
                }
                break;
            }

            case PagingAlgorithm::Optimal: {
                //find the farthest used
                int farthest_use = -1;
                for (int i = 0; i < frame_count; ++i) {
                    int next_use = std::numeric_limits<int>::max();
                    for (int j = step_index + 1; j < static_cast<int>(references.size()); ++j) {
                        if (references[j] == frames[i]) {
                            next_use = j;
                            break;
                        }
                    }
                    if (next_use > farthest_use) {
                        farthest_use = next_use;
                        target_frame = i;
                    }
                }
                break;
            }

            case PagingAlgorithm::SecondChance:
                while (true) {
                    //if 0, replace 
                    if (reference_bits[second_chance_pointer] == 0) {
                        target_frame = second_chance_pointer;
                        second_chance_pointer = (second_chance_pointer + 1) % frame_count;
                        break;
                    }
                    reference_bits[second_chance_pointer] = 0;
                    second_chance_pointer = (second_chance_pointer + 1) % frame_count;
                }
                break;
            }
        }

        step.replaced_page = frames[target_frame];
        frames[target_frame] = page;
        last_used[target_frame] = step_index;
        reference_bits[target_frame] = 1;

        if (algorithm == PagingAlgorithm::FIFO && step.replaced_page == -1) {
            fifo_pointer = (target_frame + 1) % frame_count;
        }

        std::ostringstream details;
        if (step.replaced_page == -1) {
            details << "Page fault. Loaded page " << page << " into free frame " << target_frame << ".";
        } else {
            details << "Page fault. Replaced page " << step.replaced_page << " with page " << page
                    << " in frame " << target_frame << " using " << result.algorithm_name << ".";
        }

        step.details = details.str();
        step.frames = frames;
        step.reference_bits = reference_bits;
        result.steps.push_back(step);
    }

    const int total = result.hits + result.faults;
    result.hit_ratio = total > 0 ? static_cast<double>(result.hits) * 100.0 / static_cast<double>(total) : 0.0;
    return result;
}

std::string PagingSimulator::algorithm_name(PagingAlgorithm algorithm) {
    switch (algorithm) {
    case PagingAlgorithm::FIFO:
        return "FIFO";
    case PagingAlgorithm::LRU:
        return "LRU";
    case PagingAlgorithm::Optimal:
        return "Optimal";
    case PagingAlgorithm::SecondChance:
        return "Second Chance";
    }
    return "Unknown";
}

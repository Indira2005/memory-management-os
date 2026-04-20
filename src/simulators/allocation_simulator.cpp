#include "allocation_simulator.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

//MemoryBlock and AllocationMetrics defined in models.hpp
namespace {
AllocationMetrics build_metrics(const std::vector<MemoryBlock>& blocks, int total_memory) {
    AllocationMetrics metrics;

    for (const auto& block : blocks) {
        if (block.is_free()) {
            //if the block is free, add it to the total free memory
            metrics.total_free_memory += block.size;
            //find the largest hole
            metrics.largest_hole = std::max(metrics.largest_hole, block.size);
        } else {
            //if block is used, add it to used memory
            metrics.used_memory += block.size;
        }
    }

    //ext. frag. - memory is free but not in the largestt hole
    metrics.external_fragmentation =
        metrics.total_free_memory > 0
            ? metrics.total_free_memory - metrics.largest_hole
            : 0;
    //variable partitioning - no internal fragmentation
    metrics.internal_fragmentation = 0;
    metrics.utilization =
        total_memory > 0
            ? static_cast<double>(metrics.used_memory) * 100.0 / static_cast<double>(total_memory)
            : 0.0;

    return metrics;
}

void merge_free_blocks(std::vector<MemoryBlock>& blocks) {
    //no blocks available, nothing to merge
    if (blocks.empty()) {
        return;
    }

    //start with the first block
    std::vector<MemoryBlock> merged;
    merged.push_back(blocks.front());

    //traverse the remaining blocks
    for (std::size_t i = 1; i < blocks.size(); ++i) {
        auto& last = merged.back();
        //if prev block and current block free, add the current block to the last block - merged 
        if (last.is_free() && blocks[i].is_free()) {
            last.size += blocks[i].size;
        } else {
            merged.push_back(blocks[i]);
        }
    }

    blocks = merged;
}

int select_block(
    AllocationAlgorithm algorithm,
    const std::vector<MemoryBlock>& blocks,
    int process_size,
    int& next_fit_cursor
) {
    if (blocks.empty()) {
        return -1;
    }

    switch (algorithm) {
    //allocate the first block that can accomodate the process
    case AllocationAlgorithm::FirstFit:
        for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
            //block is free and size is greater than process size
            if (blocks[i].is_free() && blocks[i].size >= process_size) {
                return i;
            }
        }
        return -1;

    //allocate the block that will leave the least amt of space 
    case AllocationAlgorithm::BestFit: {
        int best_index = -1;
        int best_size = 0;
        for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
            if (!blocks[i].is_free() || blocks[i].size < process_size) {
                continue;
            }

            //among valid blocks, choose the one with the least size 
            if (best_index == -1 || blocks[i].size < best_size) {
                best_index = i;
                best_size = blocks[i].size;
            }
        }
        return best_index;
    }

    //allocate the block that will leave the most amount of space
    case AllocationAlgorithm::WorstFit: {
        int worst_index = -1;
        int worst_size = -1;
        for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
            if (!blocks[i].is_free() || blocks[i].size < process_size) {
                continue;
            }
            //among the valid blocks, choose the one with the max size
            if (blocks[i].size > worst_size) {
                worst_index = i;
                worst_size = blocks[i].size;
            }
        }
        return worst_index;
    }


    //begin searching from the last allocated block
    case AllocationAlgorithm::NextFit:
        for (int offset = 0; offset < static_cast<int>(blocks.size()); ++offset) {
            //wrap around 
            const int index = (next_fit_cursor + offset) % static_cast<int>(blocks.size());
            if (blocks[index].is_free() && blocks[index].size >= process_size) {
                next_fit_cursor = index;
                return index;
            }
        }
        return -1;
    }

    return -1;
}
}  // namespace

AllocationResult AllocationSimulator::simulate(
    AllocationAlgorithm algorithm,
    int total_memory,
    const std::vector<AllocationOperation>& operations
) {
    if (total_memory <= 0) {
        throw std::runtime_error("Total memory must be greater than 0.");
    }

    AllocationResult result;
    result.algorithm_name = algorithm_name(algorithm);
    result.total_memory = total_memory;

    std::vector<MemoryBlock> blocks = {{0, total_memory, ""}};
    int next_fit_cursor = 0;

    result.steps.push_back({
        "Initial State",
        "info",
        "The memory map starts with one free block that covers the entire RAM.",
        blocks,
        build_metrics(blocks, total_memory),
    });

    for (const auto& operation : operations) {
        if (operation.type == AllocationOperation::Type::Allocate) {
            if (operation.size <= 0) {
                throw std::runtime_error("Allocation size must be greater than 0.");
            }

            const auto duplicate = std::find_if(
                blocks.begin(),
                blocks.end(),
                [&](const MemoryBlock& block) { return block.allocated_to == operation.pid; }
            );
            if (duplicate != blocks.end()) {
                throw std::runtime_error(
                    "Duplicate process ID detected. Each allocated process must have a unique ID."
                );
            }

            const int block_index = select_block(algorithm, blocks, operation.size, next_fit_cursor);
            if (block_index < 0) {
                result.failed_allocations++;
                const AllocationMetrics metrics = build_metrics(blocks, total_memory);
                std::ostringstream details;
                details << "Allocation failed for " << operation.pid << " (" << operation.size
                        << " KB). ";

                if (metrics.total_free_memory >= operation.size) {
                    details << "There is enough total free memory, but it is split into smaller holes. "
                            << "This is an example of external fragmentation.";
                } else {
                    details << "There is not enough free memory remaining for this request.";
                }

                result.steps.push_back({
                    "Allocation Failed",
                    "error",
                    details.str(),
                    blocks,
                    metrics,
                });
                continue;
            }

            const MemoryBlock chosen = blocks[block_index];
            std::vector<MemoryBlock> replacement = {
                {chosen.start, operation.size, operation.pid}
            };
            const int remainder = chosen.size - operation.size;
            if (remainder > 0) {
                replacement.push_back({chosen.start + operation.size, remainder, ""});
            }

            blocks.erase(blocks.begin() + block_index);
            blocks.insert(blocks.begin() + block_index, replacement.begin(), replacement.end());
            next_fit_cursor = std::min(block_index + 1, static_cast<int>(blocks.size()) - 1);
            result.successful_allocations++;

            std::ostringstream details;
            details << operation.pid << " (" << operation.size << " KB) was placed starting at address "
                    << chosen.start << " using " << result.algorithm_name << ".";
            result.steps.push_back({
                "Process Allocated",
                "success",
                details.str(),
                blocks,
                build_metrics(blocks, total_memory),
            });
        } else {
            auto it = std::find_if(
                blocks.begin(),
                blocks.end(),
                [&](const MemoryBlock& block) { return block.allocated_to == operation.pid; }
            );

            if (it == blocks.end()) {
                std::ostringstream details;
                details << "Free request ignored because process " << operation.pid
                        << " is not currently allocated.";
                result.steps.push_back({
                    "Free Ignored",
                    "warning",
                    details.str(),
                    blocks,
                    build_metrics(blocks, total_memory),
                });
                continue;
            }

            it->allocated_to.clear();
            merge_free_blocks(blocks);
            next_fit_cursor = 0;

            std::ostringstream details;
            details << operation.pid << " was released. Adjacent free blocks were merged to reduce fragmentation.";
            result.steps.push_back({
                "Process Freed",
                "success",
                details.str(),
                blocks,
                build_metrics(blocks, total_memory),
            });
        }
    }

    result.final_metrics = build_metrics(blocks, total_memory);
    return result;
}

std::string AllocationSimulator::algorithm_name(AllocationAlgorithm algorithm) {
    switch (algorithm) {
    case AllocationAlgorithm::FirstFit:
        return "First Fit";
    case AllocationAlgorithm::BestFit:
        return "Best Fit";
    case AllocationAlgorithm::WorstFit:
        return "Worst Fit";
    case AllocationAlgorithm::NextFit:
        return "Next Fit";
    }
    return "Unknown";
}

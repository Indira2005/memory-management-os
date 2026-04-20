#include "demand_paging_simulator.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

DemandPagingResult DemandPagingSimulator::simulate(
    int frame_count,
    int tlb_size,
    const std::vector<int>& references
) {
    if (frame_count <= 0) {
        throw std::runtime_error("Frame count must be greater than 0.");
    }
    if (tlb_size < 0) {
        throw std::runtime_error("TLB size must not be negative.");
    }
    if (references.empty()) {
        throw std::runtime_error("Reference string must not be empty.");
    }

    DemandPagingResult result;
    result.frame_count = frame_count;
    result.tlb_size = tlb_size;
    result.reference_string = references;

    std::vector<int> frames(frame_count, -1);
    std::vector<TlbEntry> tlb;
    int frame_pointer = 0;
    int tlb_pointer = 0;

    for (int page : references) {
        DemandPagingStep step;
        step.reference = page;

        const auto tlb_it = std::find_if(
            tlb.begin(),
            tlb.end(),
            [&](const TlbEntry& entry) { return entry.page == page; }
        );

        if (tlb_it != tlb.end()) {
            step.tlb_hit = true;
            step.page_hit = true;
            result.tlb_hits++;
            result.page_hits++;
            step.details = "TLB hit. The page-to-frame mapping was found without consulting the page table.";
            step.frames = frames;
            step.tlb = tlb;
            result.steps.push_back(step);
            continue;
        }

        const auto frame_it = std::find(frames.begin(), frames.end(), page);
        if (frame_it != frames.end()) {
            step.page_hit = true;
            result.page_hits++;

            if (tlb_size > 0) {
                if (static_cast<int>(tlb.size()) < tlb_size) {
                    tlb.push_back({page, static_cast<int>(frame_it - frames.begin())});
                } else {
                    tlb[tlb_pointer] = {page, static_cast<int>(frame_it - frames.begin())};
                    tlb_pointer = (tlb_pointer + 1) % tlb_size;
                }
            }

            step.details = "Page-table hit. The page was already in memory, so only the TLB was updated.";
            step.frames = frames;
            step.tlb = tlb;
            result.steps.push_back(step);
            continue;
        }

        step.page_fault = true;
        result.page_faults++;

        int target_frame = -1;
        for (int i = 0; i < frame_count; ++i) {
            if (frames[i] == -1) {
                target_frame = i;
                break;
            }
        }

        if (target_frame == -1) {
            target_frame = frame_pointer;
            step.replaced_page = frames[target_frame];
            frame_pointer = (frame_pointer + 1) % frame_count;
        }

        frames[target_frame] = page;
        step.loaded_into_frame = target_frame;

        if (tlb_size > 0) {
            auto stale = std::remove_if(
                tlb.begin(),
                tlb.end(),
                [&](const TlbEntry& entry) { return entry.page == step.replaced_page; }
            );
            tlb.erase(stale, tlb.end());

            if (static_cast<int>(tlb.size()) < tlb_size) {
                tlb.push_back({page, target_frame});
            } else {
                tlb[tlb_pointer] = {page, target_frame};
                tlb_pointer = (tlb_pointer + 1) % tlb_size;
            }
        }

        std::ostringstream details;
        if (step.replaced_page == -1) {
            details << "Page fault. Page " << page << " was brought from backing store into free frame "
                    << target_frame << ".";
        } else {
            details << "Page fault. Page " << page << " was brought from backing store and page "
                    << step.replaced_page << " was swapped out of frame " << target_frame << ".";
        }
        step.details = details.str();
        step.frames = frames;
        step.tlb = tlb;
        result.steps.push_back(step);
    }

    return result;
}

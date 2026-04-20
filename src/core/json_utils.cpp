#include "json_utils.hpp"

#include <iomanip>
#include <sstream>

namespace {
std::string format_double(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

std::string metrics_json(const AllocationMetrics& metrics) {
    std::ostringstream out;
    out << "{"
        << "\"usedMemory\":" << metrics.used_memory << ","
        << "\"totalFreeMemory\":" << metrics.total_free_memory << ","
        << "\"largestHole\":" << metrics.largest_hole << ","
        << "\"externalFragmentation\":" << metrics.external_fragmentation << ","
        << "\"internalFragmentation\":" << metrics.internal_fragmentation << ","
        << "\"utilization\":" << format_double(metrics.utilization)
        << "}";
    return out.str();
}
}  // namespace

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (char ch : value) {
        switch (ch) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << ch;
            break;
        }
    }
    return out.str();
}

std::string to_json(const AllocationResult& result) {
    std::ostringstream out;
    out << "{"
        << "\"algorithm\":\"" << json_escape(result.algorithm_name) << "\","
        << "\"totalMemory\":" << result.total_memory << ","
        << "\"successfulAllocations\":" << result.successful_allocations << ","
        << "\"failedAllocations\":" << result.failed_allocations << ","
        << "\"finalMetrics\":" << metrics_json(result.final_metrics) << ","
        << "\"steps\":[";

    for (std::size_t i = 0; i < result.steps.size(); ++i) {
        const auto& step = result.steps[i];
        if (i > 0) {
            out << ",";
        }
        out << "{"
            << "\"title\":\"" << json_escape(step.title) << "\","
            << "\"status\":\"" << json_escape(step.status) << "\","
            << "\"details\":\"" << json_escape(step.details) << "\","
            << "\"metrics\":" << metrics_json(step.metrics) << ","
            << "\"blocks\":[";

        for (std::size_t j = 0; j < step.blocks.size(); ++j) {
            const auto& block = step.blocks[j];
            if (j > 0) {
                out << ",";
            }
            out << "{"
                << "\"start\":" << block.start << ","
                << "\"end\":" << block.end() << ","
                << "\"size\":" << block.size << ","
                << "\"owner\":\"" << json_escape(block.is_free() ? "FREE" : block.allocated_to) << "\","
                << "\"free\":" << (block.is_free() ? "true" : "false")
                << "}";
        }

        out << "]"
            << "}";
    }

    out << "]"
        << "}";
    return out.str();
}

std::string to_json(const PagingResult& result) {
    std::ostringstream out;
    out << "{"
        << "\"algorithm\":\"" << json_escape(result.algorithm_name) << "\","
        << "\"frameCount\":" << result.frame_count << ","
        << "\"hits\":" << result.hits << ","
        << "\"faults\":" << result.faults << ","
        << "\"hitRatio\":" << format_double(result.hit_ratio) << ","
        << "\"referenceString\":[";

    for (std::size_t i = 0; i < result.reference_string.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << result.reference_string[i];
    }

    out << "],\"steps\":[";

    for (std::size_t i = 0; i < result.steps.size(); ++i) {
        const auto& step = result.steps[i];
        if (i > 0) {
            out << ",";
        }
        out << "{"
            << "\"page\":" << step.reference << ","
            << "\"hit\":" << (step.hit ? "true" : "false") << ","
            << "\"pageFault\":" << (step.page_fault ? "true" : "false") << ","
            << "\"replacedPage\":" << step.replaced_page << ","
            << "\"details\":\"" << json_escape(step.details) << "\","
            << "\"frames\":[";

        for (std::size_t j = 0; j < step.frames.size(); ++j) {
            if (j > 0) {
                out << ",";
            }
            out << step.frames[j];
        }

        out << "],\"referenceBits\":[";

        for (std::size_t j = 0; j < step.reference_bits.size(); ++j) {
            if (j > 0) {
                out << ",";
            }
            out << step.reference_bits[j];
        }

        out << "]"
            << "}";
    }

    out << "]"
        << "}";
    return out.str();
}

std::string to_json(const DemandPagingResult& result) {
    std::ostringstream out;
    out << "{"
        << "\"frameCount\":" << result.frame_count << ","
        << "\"tlbSize\":" << result.tlb_size << ","
        << "\"tlbHits\":" << result.tlb_hits << ","
        << "\"pageHits\":" << result.page_hits << ","
        << "\"pageFaults\":" << result.page_faults << ","
        << "\"referenceString\":[";
    for (std::size_t i = 0; i < result.reference_string.size(); ++i) {
        if (i > 0) out << ",";
        out << result.reference_string[i];
    }
    out << "],\"steps\":[";
    for (std::size_t i = 0; i < result.steps.size(); ++i) {
        const auto& step = result.steps[i];
        if (i > 0) out << ",";
        out << "{"
            << "\"page\":" << step.reference << ","
            << "\"tlbHit\":" << (step.tlb_hit ? "true" : "false") << ","
            << "\"pageHit\":" << (step.page_hit ? "true" : "false") << ","
            << "\"pageFault\":" << (step.page_fault ? "true" : "false") << ","
            << "\"loadedIntoFrame\":" << step.loaded_into_frame << ","
            << "\"replacedPage\":" << step.replaced_page << ","
            << "\"details\":\"" << json_escape(step.details) << "\","
            << "\"frames\":[";
        for (std::size_t j = 0; j < step.frames.size(); ++j) {
            if (j > 0) out << ",";
            out << step.frames[j];
        }
        out << "],\"tlb\":[";
        for (std::size_t j = 0; j < step.tlb.size(); ++j) {
            if (j > 0) out << ",";
            out << "{"
                << "\"page\":" << step.tlb[j].page << ","
                << "\"frame\":" << step.tlb[j].frame
                << "}";
        }
        out << "]}";
    }
    out << "]}";
    return out.str();
}

std::string to_json(const MultiLevelPagingResult& result) {
    std::ostringstream out;
    out << "{"
        << "\"pageSize\":" << result.page_size << ","
        << "\"logicalAddress\":" << result.logical_address << ","
        << "\"pageNumber\":" << result.page_number << ","
        << "\"outerIndex\":" << result.outer_index << ","
        << "\"innerIndex\":" << result.inner_index << ","
        << "\"offset\":" << result.offset << ","
        << "\"innerBits\":" << result.inner_bits << ","
        << "\"found\":" << (result.found ? "true" : "false") << ","
        << "\"frameNumber\":" << result.frame_number << ","
        << "\"physicalAddress\":" << result.physical_address << ","
        << "\"details\":\"" << json_escape(result.details) << "\","
        << "\"pageMap\":[";
    for (std::size_t i = 0; i < result.page_map.size(); ++i) {
        if (i > 0) out << ",";
        out << "{"
            << "\"outerIndex\":" << result.page_map[i].outer_index << ","
            << "\"innerIndex\":" << result.page_map[i].inner_index << ","
            << "\"frame\":" << result.page_map[i].frame << ","
            << "\"present\":" << (result.page_map[i].present ? "true" : "false")
            << "}";
    }
    out << "]}";
    return out.str();
}

std::string to_json(const TranslationResult& result) {
    std::ostringstream out;
    out << "{"
        << "\"pageSize\":" << result.page_size << ","
        << "\"logicalAddress\":" << result.logical_address << ","
        << "\"pageNumber\":" << result.page_number << ","
        << "\"offset\":" << result.offset << ","
        << "\"found\":" << (result.found ? "true" : "false") << ","
        << "\"frameNumber\":" << result.frame_number << ","
        << "\"physicalAddress\":" << result.physical_address << ","
        << "\"details\":\"" << json_escape(result.details) << "\","
        << "\"pageTable\":[";

    for (std::size_t i = 0; i < result.page_table.size(); ++i) {
        const auto& entry = result.page_table[i];
        if (i > 0) {
            out << ",";
        }
        out << "{"
            << "\"page\":" << entry.page << ","
            << "\"frame\":" << entry.frame << ","
            << "\"present\":" << (entry.present ? "true" : "false")
            << "}";
    }

    out << "]"
        << "}";
    return out.str();
}

std::string to_json(const TlbTranslationResult& result) {
    std::ostringstream out;
    out << "{"
        << "\"pageSize\":" << result.page_size << ","
        << "\"logicalAddress\":" << result.logical_address << ","
        << "\"pageNumber\":" << result.page_number << ","
        << "\"offset\":" << result.offset << ","
        << "\"tlbHit\":" << (result.tlb_hit ? "true" : "false") << ","
        << "\"pageFound\":" << (result.page_found ? "true" : "false") << ","
        << "\"frameNumber\":" << result.frame_number << ","
        << "\"physicalAddress\":" << result.physical_address << ","
        << "\"details\":\"" << json_escape(result.details) << "\","
        << "\"tlb\":[";
    for (std::size_t i = 0; i < result.tlb.size(); ++i) {
        if (i > 0) out << ",";
        out << "{"
            << "\"page\":" << result.tlb[i].page << ","
            << "\"frame\":" << result.tlb[i].frame
            << "}";
    }
    out << "],\"pageTable\":[";
    for (std::size_t i = 0; i < result.page_table.size(); ++i) {
        if (i > 0) out << ",";
        out << "{"
            << "\"page\":" << result.page_table[i].page << ","
            << "\"frame\":" << result.page_table[i].frame << ","
            << "\"present\":" << (result.page_table[i].present ? "true" : "false")
            << "}";
    }
    out << "]}";
    return out.str();
}

std::string to_json(const SegmentationResult& result) {
    std::ostringstream out;
    out << "{"
        << "\"logicalAddress\":" << result.logical_address << ","
        << "\"bitsForOffset\":" << result.bits_for_offset << ","
        << "\"segmentNumber\":" << result.segment_number << ","
        << "\"offset\":" << result.offset << ","
        << "\"valid\":" << (result.valid ? "true" : "false") << ","
        << "\"physicalAddress\":" << result.physical_address << ","
        << "\"details\":\"" << json_escape(result.details) << "\","
        << "\"segmentTable\":[";
    for (std::size_t i = 0; i < result.segment_table.size(); ++i) {
        if (i > 0) out << ",";
        out << "{"
            << "\"segment\":" << result.segment_table[i].segment << ","
            << "\"base\":" << result.segment_table[i].base << ","
            << "\"limit\":" << result.segment_table[i].limit
            << "}";
    }
    out << "]}";
    return out.str();
}

std::string to_json(const ThrashingResult& result) {
    std::ostringstream out;
    out << "{"
        << "\"algorithm\":\"" << json_escape(result.algorithm_name) << "\","
        << "\"detected\":" << (result.detected ? "true" : "false") << ","
        << "\"threshold\":" << result.thrashing_threshold << ","
        << "\"details\":\"" << json_escape(result.details) << "\","
        << "\"points\":[";
    for (std::size_t i = 0; i < result.points.size(); ++i) {
        if (i > 0) out << ",";
        out << "{"
            << "\"frameCount\":" << result.points[i].frame_count << ","
            << "\"faults\":" << result.points[i].faults << ","
            << "\"faultRate\":" << format_double(result.points[i].fault_rate)
            << "}";
    }
    out << "]}";
    return out.str();
}

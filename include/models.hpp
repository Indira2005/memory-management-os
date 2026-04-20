#ifndef MODELS_HPP
#define MODELS_HPP

#include <string>
#include <vector>

enum class AllocationAlgorithm {
    FirstFit,
    BestFit,
    WorstFit,
    NextFit,
};

enum class PagingAlgorithm {
    FIFO,
    LRU,
    Optimal,
    SecondChance,
};

struct MemoryBlock {
    int start = 0;
    int size = 0;
    std::string allocated_to;

    bool is_free() const {
        return allocated_to.empty();
    }

    int end() const {
        return start + size - 1;
    }
};

struct AllocationOperation {
    enum class Type {
        Allocate,
        Free,
    };

    Type type = Type::Allocate;
    std::string pid;
    int size = 0;
};

struct AllocationMetrics {
    int used_memory = 0;
    int total_free_memory = 0;
    int largest_hole = 0;
    int external_fragmentation = 0;
    int internal_fragmentation = 0;
    double utilization = 0.0;
};

struct AllocationStep {
    std::string title;
    std::string status;
    std::string details;
    std::vector<MemoryBlock> blocks;
    AllocationMetrics metrics;
};

struct AllocationResult {
    std::string algorithm_name;
    int total_memory = 0;
    int successful_allocations = 0;
    int failed_allocations = 0;
    std::vector<AllocationStep> steps;
    AllocationMetrics final_metrics;
};

struct PagingStep {
    int reference = -1;
    bool hit = false;
    bool page_fault = false;
    int replaced_page = -1;
    std::vector<int> frames;
    std::vector<int> reference_bits;
    std::string details;
};

struct PagingResult {
    std::string algorithm_name;
    int frame_count = 0;
    std::vector<int> reference_string;
    int hits = 0;
    int faults = 0;
    double hit_ratio = 0.0;
    std::vector<PagingStep> steps;
};

struct TranslationEntry {
    int page = 0;
    int frame = -1;
    bool present = false;
};

struct TranslationResult {
    int page_size = 0;
    int logical_address = 0;
    int page_number = 0;
    int offset = 0;
    bool found = false;
    int frame_number = -1;
    int physical_address = -1;
    std::string details;
    std::vector<TranslationEntry> page_table;
};

struct TlbEntry {
    int page = 0;
    int frame = -1;
};

struct TlbTranslationResult {
    int page_size = 0;
    int logical_address = 0;
    int page_number = 0;
    int offset = 0;
    bool tlb_hit = false;
    bool page_found = false;
    int frame_number = -1;
    int physical_address = -1;
    std::string details;
    std::vector<TlbEntry> tlb;
    std::vector<TranslationEntry> page_table;
};

struct DemandPagingStep {
    int reference = -1;
    bool tlb_hit = false;
    bool page_hit = false;
    bool page_fault = false;
    int loaded_into_frame = -1;
    int replaced_page = -1;
    std::vector<int> frames;
    std::vector<TlbEntry> tlb;
    std::string details;
};

struct DemandPagingResult {
    int frame_count = 0;
    int tlb_size = 0;
    std::vector<int> reference_string;
    int tlb_hits = 0;
    int page_hits = 0;
    int page_faults = 0;
    std::vector<DemandPagingStep> steps;
};

struct SegmentEntry {
    int segment = 0;
    int base = 0;
    int limit = 0;
};

struct SegmentationResult {
    int logical_address = 0;
    int bits_for_offset = 0;
    int segment_number = 0;
    int offset = 0;
    bool valid = false;
    int physical_address = -1;
    std::string details;
    std::vector<SegmentEntry> segment_table;
};

struct ThrashingPoint {
    int frame_count = 0;
    int faults = 0;
    double fault_rate = 0.0;
};

struct ThrashingResult {
    std::string algorithm_name;
    std::vector<int> reference_string;
    std::vector<ThrashingPoint> points;
    int thrashing_threshold = 0;
    bool detected = false;
    std::string details;
};

struct MultiLevelEntry {
    int outer_index = 0;
    int inner_index = 0;
    int frame = -1;
    bool present = false;
};

struct MultiLevelPagingResult {
    int page_size = 0;
    int logical_address = 0;
    int page_number = 0;
    int outer_index = 0;
    int inner_index = 0;
    int offset = 0;
    int inner_bits = 0;
    bool found = false;
    int frame_number = -1;
    int physical_address = -1;
    std::string details;
    std::vector<MultiLevelEntry> page_map;
};

#endif

#include "translation_simulator.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

TranslationResult TranslationSimulator::simulate(
    int page_size,
    int logical_address,
    const std::vector<TranslationEntry>& page_table
) {
    if (page_size <= 0) {
        throw std::runtime_error("Page size must be greater than 0.");
    }
    if (logical_address < 0) {
        throw std::runtime_error("Logical address must not be negative.");
    }

    TranslationResult result;
    result.page_size = page_size;
    result.logical_address = logical_address;
    result.page_number = logical_address / page_size;
    result.offset = logical_address % page_size;
    result.page_table = page_table;

    for (const auto& entry : page_table) {
        //search till page is found
        if (entry.page != result.page_number) {
            continue;
        }

        //page found but no frame
        if (!entry.present || entry.frame < 0) {
            result.details = "The page-table entry exists, but the page is not currently loaded in main memory.";
            return result;
        }

        //found
        result.found = true;
        result.frame_number = entry.frame;
        result.physical_address = entry.frame * page_size + result.offset;

        std::ostringstream details;
        details << "Logical address " << logical_address << " maps to page " << result.page_number
                << " with offset " << result.offset << ". The page is in frame " << entry.frame
                << ", so the physical address is " << result.physical_address << ".";
        result.details = details.str();
        return result;
    }

    result.details = "No page-table entry was found for the calculated page number, so translation cannot continue.";
    return result;
}

TlbTranslationResult TranslationSimulator::simulate_with_tlb(
    int page_size,
    int logical_address,
    const std::vector<TranslationEntry>& page_table,
    const std::vector<TlbEntry>& tlb
) {
    if (page_size <= 0) {
        throw std::runtime_error("Page size must be greater than 0.");
    }
    if (logical_address < 0) {
        throw std::runtime_error("Logical address must not be negative.");
    }

    TlbTranslationResult result;
    result.page_size = page_size;
    result.logical_address = logical_address;
    result.page_number = logical_address / page_size;
    result.offset = logical_address % page_size;
    result.page_table = page_table;
    result.tlb = tlb;

    const auto tlb_it = std::find_if(
        tlb.begin(),
        tlb.end(),
        [&](const TlbEntry& entry) { return entry.page == result.page_number; }
    );
    if (tlb_it != tlb.end()) {
        result.tlb_hit = true;
        result.page_found = true;
        result.frame_number = tlb_it->frame;
        result.physical_address = tlb_it->frame * page_size + result.offset;

        std::ostringstream details;
        details << "TLB hit for page " << result.page_number << ". The frame " << tlb_it->frame
                << " was found immediately, so the physical address is " << result.physical_address << ".";
        result.details = details.str();
        return result;
    }

    const TranslationResult base = simulate(page_size, logical_address, page_table);
    result.page_found = base.found;
    result.frame_number = base.frame_number;
    result.physical_address = base.physical_address;
    result.details = base.found
        ? "TLB miss. The page table was consulted and the page was found in memory. " + base.details
        : "TLB miss. " + base.details;
    return result;
}

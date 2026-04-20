#ifndef TRANSLATION_SIMULATOR_HPP
#define TRANSLATION_SIMULATOR_HPP

#include <vector>

#include "models.hpp"

class TranslationSimulator {
public:
    TranslationResult simulate(
        int page_size,
        int logical_address,
        const std::vector<TranslationEntry>& page_table
    );

    TlbTranslationResult simulate_with_tlb(
        int page_size,
        int logical_address,
        const std::vector<TranslationEntry>& page_table,
        const std::vector<TlbEntry>& tlb
    );
};

#endif

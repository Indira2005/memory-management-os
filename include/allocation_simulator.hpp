//check if the file is not defined, if not define it 
//prevemts multiple inclusions, include the file only once 
#ifndef ALLOCATION_SIMULATOR_HPP 
#define ALLOCATION_SIMULATOR_HPP

#include <string>
#include <vector>

#include "models.hpp"

class AllocationSimulator {
public:
    AllocationResult simulate(
        AllocationAlgorithm algorithm,
        int total_memory,
        const std::vector<AllocationOperation>& operations
    );

    static std::string algorithm_name(AllocationAlgorithm algorithm);
};

#endif

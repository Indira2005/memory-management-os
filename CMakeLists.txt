cmake_minimum_required(VERSION 3.14)
project(OSMemoryManagementSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(simulator
    src/main.cpp
    src/core/http_server.cpp
    src/core/json_utils.cpp
    src/core/utils.cpp
    src/core/web_assets.cpp
    src/simulators/allocation_simulator.cpp
    src/simulators/demand_paging_simulator.cpp
    src/simulators/multi_level_paging_simulator.cpp
    src/simulators/paging_simulator.cpp
    src/simulators/segmentation_simulator.cpp
    src/simulators/thrashing_simulator.cpp
    src/simulators/translation_simulator.cpp
)

if (WIN32)
    target_link_libraries(simulator PRIVATE ws2_32)
endif()

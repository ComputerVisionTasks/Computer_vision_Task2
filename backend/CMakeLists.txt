cmake_minimum_required(VERSION 3.14)
project(FromScratchCV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server
    main.cpp
    algorithms/image_utils.cpp
    algorithms/edge_detection.cpp
    algorithms/shape_detection.cpp
    algorithms/active_contour.cpp
    algorithms/contour_analysis.cpp
)

target_include_directories(server PRIVATE include ${CMAKE_SOURCE_DIR})

# Windows: need ws2_32 for sockets (httplib)
if(WIN32)
    target_link_libraries(server PRIVATE ws2_32)
endif()

cmake_minimum_required(VERSION 3.5)
project(PiBench)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenMP REQUIRED)

######################## Intel PCM ########################
add_custom_command(OUTPUT libPCM.a
                    COMMAND make lib
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/pcm)

add_custom_target(pcm DEPENDS libPCM.a)
###########################################################

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

include_directories("${PROJECT_SOURCE_DIR}/include"
                    "${PROJECT_SOURCE_DIR}/pcm")

# Example libraries
add_library(dummy_wrapper SHARED wrappers/dummy/dummy_wrapper.cpp)
add_library(stlmap_wrapper SHARED wrappers/stlmap/stlmap_wrapper.cpp)

set(benchmark_SRC
    src/key_generator.cpp
    src/library_loader.cpp
    src/main.cpp
    src/benchmark.cpp
    src/operation_generator.cpp
    src/value_generator.cpp
)

add_executable(PiBench ${benchmark_SRC})
add_dependencies(PiBench pcm)
target_compile_options(PiBench PRIVATE ${OpenMP_CXX_FLAGS})
target_link_libraries(PiBench PRIVATE
    ${OpenMP_CXX_FLAGS}
    ${PROJECT_SOURCE_DIR}/pcm/libPCM.a
    dl)
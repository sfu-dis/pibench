cmake_minimum_required(VERSION 3.14)
project(PiBench)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

option(PIBENCH_BUILD_LEVELDB "Build LevelDB wrapper" OFF)
option(PIBENCH_USE_EPOCH_BASED_RECLAMATION "Use Epoch-based Reclamation" ON)

include(CTest)

find_package(OpenMP REQUIRED)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(OpenMP_C "${CMAKE_C_COMPILER}")
  set(OpenMP_C_FLAGS "-fopenmp=libomp")
  set(OpenMP_C_LIB_NAMES "libomp" "libgomp" "libiomp5")
  set(OpenMP_libomp_LIBRARY ${OpenMP_C_LIB_NAMES})
  set(OpenMP_libgomp_LIBRARY ${OpenMP_C_LIB_NAMES})
  set(OpenMP_libiomp5_LIBRARY ${OpenMP_C_LIB_NAMES})
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(OpenMP_CXX "${CMAKE_CXX_COMPILER}")
  set(OpenMP_CXX_FLAGS "-fopenmp=libomp")
  set(OpenMP_CXX_LIB_NAMES "libomp" "libgomp" "libiomp5")
  set(OpenMP_libomp_LIBRARY ${OpenMP_CXX_LIB_NAMES})
  set(OpenMP_libgomp_LIBRARY ${OpenMP_CXX_LIB_NAMES})
  set(OpenMP_libiomp5_LIBRARY ${OpenMP_CXX_LIB_NAMES})
endif()

######################## Intel PCM ########################
add_custom_command(OUTPUT libPCM.a
                    COMMAND make lib
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/pcm)

add_custom_target(pcm DEPENDS libPCM.a)
###########################################################

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

include_directories("${PROJECT_SOURCE_DIR}/include"
                    "${PROJECT_SOURCE_DIR}/pcm")

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(googletest)
    add_subdirectory(tests)
endif()

if(PIBENCH_BUILD_LEVELDB)
  add_subdirectory(wrappers/leveldb)
endif(PIBENCH_BUILD_LEVELDB)

# Example libraries
add_library(dummy_wrapper SHARED wrappers/dummy/dummy_wrapper.cpp)
add_library(stlmap_wrapper SHARED wrappers/stlmap/stlmap_wrapper.cpp)

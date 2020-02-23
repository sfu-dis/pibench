cmake_minimum_required(VERSION 3.5)
project(PiBench)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

#find_package(OpenMP REQUIRED)
find_library(OpenMP_LIBRARY
    NAMES omp
)
find_path(OpenMP_INCLUDE_DIR
    omp.h
)
mark_as_advanced(OpenMP_LIBRARY OpenMP_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenMP DEFAULT_MSG 
    OpenMP_LIBRARY OpenMP_INCLUDE_DIR)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(OpenMP_C "${CMAKE_C_COMPILER}")
  set(OpenMP_C_FLAGS "-fopenmp=libomp")
  set(OpenMP_C_LIB_NAMES "libomp" "libgomp" "libiomp5" "llvm-openmp")
  set(OpenMP_libomp_LIBRARY ${OpenMP_C_LIB_NAMES})
  set(OpenMP_libgomp_LIBRARY ${OpenMP_C_LIB_NAMES})
  set(OpenMP_libiomp5_LIBRARY ${OpenMP_C_LIB_NAMES})
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(OpenMP_CXX "${CMAKE_CXX_COMPILER}")
  set(OpenMP_CXX_FLAGS "-fopenmp=libomp")
  set(OpenMP_CXX_LIB_NAMES "libomp" "libgomp" "libiomp5" "llvm-openmp")
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

# Leveldb library
add_subdirectory(wrappers/leveldb)

# Example libraries
add_library(dummy_wrapper SHARED wrappers/dummy/dummy_wrapper.cpp)
add_library(stlmap_wrapper SHARED wrappers/stlmap/stlmap_wrapper.cpp)

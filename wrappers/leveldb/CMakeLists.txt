include(FetchContent)
set(LEVELDB_BUILD_TESTS OFF CACHE INTERNAL "") 
set(LEVELDB_BUILD_BENCHMARKS OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")

FetchContent_Declare(
  leveldb
  GIT_REPOSITORY https://github.com/google/leveldb.git
  GIT_TAG 1.22
)
FetchContent_MakeAvailable(leveldb)

add_library(leveldb_wrapper SHARED leveldb_wrapper.cpp)
target_link_libraries(leveldb_wrapper leveldb)
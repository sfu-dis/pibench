cmake_minimum_required (VERSION 3.13)

include_directories(${PROJECT_SOURCE_DIR}/include)

include(ExternalProject)
ExternalProject_Add(
  leveldb
  GIT_REPOSITORY https://github.com/google/leveldb.git
  GIT_TAG master 
  INSTALL_COMMAND ""  
  BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/leveldb-prefix/src/leveldb-build/${CMAKE_STATIC_LIBRARY_PREFIX}leveldb${CMAKE_STATIC_LIBRARY_SUFFIX}
  CMAKE_ARGS -DCMAKE_CXX_FLAGS="-fPIC"
)

set(LEVELDB_SRC_PATH "${CMAKE_CURRENT_BINARY_DIR}/leveldb-prefix/src/leveldb")
set(LEVELDB_LIB_PATH "${CMAKE_CURRENT_BINARY_DIR}/leveldb-prefix/src/leveldb-build")

include_directories(${LEVELDB_SRC_PATH}/include)
link_directories(${LEVELDB_LIB_PATH})

message("LevelDB source: ${LEVELDB_SRC_PATH}")
message("LevelDB library: ${LEVELDB_LIB_PATH}")

add_library(leveldb_wrapper SHARED leveldb_wrapper.cpp)
set(LEVELDB_LINK_FLAGS "-lleveldb -lpthread -lsnappy -lnuma")
target_link_libraries(leveldb_wrapper "${LEVELDB_LINK_FLAGS}")
add_dependencies(leveldb_wrapper leveldb)

# =============================================================== #
# Use other leveldb

# novellsm
#set(LEVELDB_SRC_PATH "/home/jhpark/lsm_nvm/include")
#set(LEVELDB_LIB_PATH "/home/jhpark/lsm_nvm/out-static")

# vanilla 
#set(LEVELDB_SRC_PATH "/home/jhpark/lsm_nvm/leveldb-1.20/include")
#set(LEVELDB_LIB_PATH "/home/jhpark/lsm_nvm/leveldb-1.20/out-static")

#include_directories(${LEVELDB_SRC_PATH})
#link_directories(${LEVELDB_LIB_PATH})

#add_library(leveldb_wrapper SHARED leveldb_wrapper.cpp)
#set(LEVELDB_LINK_FLAGS "-lleveldb -lpthread -lsnappy -lnuma")
#target_link_libraries(leveldb_wrapper "${LEVELDB_LINK_FLAGS}")

# =============================================================== #

set(pibench_SRC
    key_generator.cpp
    library_loader.cpp
    benchmark.cpp
    operation_generator.cpp
    value_generator.cpp
)

add_library(pibench ${pibench_SRC})
add_dependencies(pibench pcm)
target_compile_options(pibench PRIVATE ${OpenMP_CXX_FLAGS})
target_link_libraries(pibench PRIVATE
    ${OpenMP_CXX_FLAGS}
    ${PROJECT_SOURCE_DIR}/pcm/libPCM.a
    dl)

add_executable(pibench-bin main.cpp)
target_link_libraries(pibench-bin pibench)
set_target_properties(pibench-bin PROPERTIES OUTPUT_NAME PiBench)

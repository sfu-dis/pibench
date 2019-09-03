set(benchmark_SRC
    key_generator.cpp
    library_loader.cpp
    main.cpp
    benchmark.cpp
    operation_generator.cpp
    value_generator.cpp
)

add_executable(PiBench ${benchmark_SRC})
add_dependencies(PiBench pcm)
target_compile_options(PiBench PRIVATE ${OpenMP_CXX_FLAGS})
target_link_libraries(PiBench PRIVATE
    ${OpenMP_CXX_FLAGS}
    ${PROJECT_SOURCE_DIR}/pcm/libPCM.a
    dl)

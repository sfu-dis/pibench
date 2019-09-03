include(GoogleTest)

add_executable(PiBenchTests
    test_value_generator.cpp)

target_link_libraries(PiBenchTests pibench gtest gtest_main)

gtest_add_tests(TARGET PiBenchTests)

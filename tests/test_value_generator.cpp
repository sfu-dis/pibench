#include "gtest/gtest.h"
#include "value_generator.hpp"

#include <cstring>
#include <thread>

namespace
{

TEST(ValueGenerator, Simple)
{
    PiBench::value_generator_t gen(10);
    EXPECT_EQ(gen.size(), 10);

    EXPECT_EQ(gen.get_seed(), 0);
    gen.set_seed(1729);
    EXPECT_EQ(gen.get_seed(), 1729);

    auto v1 = gen.next();
    auto v2 = gen.next();
    EXPECT_NE(memcmp(v1, v2, gen.size()), 0);
}

TEST(ValueGenerator, SimpleMultithread)
{
    PiBench::value_generator_t gen(10);
    EXPECT_EQ(gen.size(), 10);

    EXPECT_EQ(gen.get_seed(), 0);
    gen.set_seed(1729);
    EXPECT_EQ(gen.get_seed(), 1729);

    auto v1 = gen.next();

    std::thread other ([&gen, &v1]()
        {
            EXPECT_EQ(gen.size(), 10);
            EXPECT_EQ(gen.get_seed(), 0);
            gen.set_seed(69);
            EXPECT_EQ(gen.get_seed(), 69);

            auto v2 = gen.next();

            // The same value has very low probability of being generated for
            // two different initial seeds.
            EXPECT_NE(memcmp(v1, v2, gen.size()), 0);
        }
    );

    other.join();
    EXPECT_EQ(gen.get_seed(), 1729);
}

TEST(ValueGenerator, ResetSeed)
{
    PiBench::value_generator_t gen(10);
    gen.set_seed(1729);
    auto v1 = gen.next();

    gen.set_seed(1729);
    auto v2 = gen.next();

    EXPECT_EQ(memcmp(v1, v2, gen.size()), 0);
}

}  // namespace

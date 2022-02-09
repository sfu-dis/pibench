#include "gtest/gtest.h"
#include "key_generator.hpp"
#include "utils.hpp"

#include <cstring>
#include <thread>
#include <unordered_set>

using namespace PiBench;

namespace
{

template <class T>
class KeyGeneratorTest : public testing::Test
{
  protected:
    KeyGeneratorTest() {}
    ~KeyGeneratorTest() override {}

    void SetUp() override
    {
        // Save initial seed_ and current_id_
        seed_ = key_generator_t::get_seed();
        current_id_ = key_generator_t::current_id_;

    }

    void TearDown() override
    {
        // Restore initial seed_ and current_id_
        key_generator_t::set_seed(seed_);
        key_generator_t::current_id_ = current_id_;
    }

    std::unique_ptr<key_generator_t>
    Instantiate(size_t N, size_t size, const std::string& prefix = "")
    {        
        return std::make_unique<T>(N, size, true, prefix);
    }

  private:
    uint32_t seed_;
    uint64_t current_id_;
};

// Declare implementations to be used for testing key generator
using uniform = uniform_key_generator_t;
using selfsimilar = selfsimilar_key_generator_t;
using zipfian = zipfian_key_generator_t;
using Implementations = ::testing::Types<uniform,selfsimilar,zipfian>;
TYPED_TEST_SUITE(KeyGeneratorTest, Implementations);


TYPED_TEST(KeyGeneratorTest, Simple)
{
    auto gen = this->Instantiate(10, 8);

    // Check keyspace size
    EXPECT_EQ(gen->keyspace(), 10);

    // Check key size
    EXPECT_EQ(gen->size(), 8);

    // Check seed
    EXPECT_EQ(gen->get_seed(), 0);
    gen->set_seed(1729);
    EXPECT_EQ(gen->get_seed(), 1729);

    std::unordered_set<uint64_t> key_space;
    // Check if keys are generated in sequence
    for(size_t i=1; i<=10; ++i) {
        const char* key = gen->next(true);
        uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
        EXPECT_EQ(key_int, PiBench::utils::multiplicative_hash<uint64_t>(i));
        
        auto r = key_space.insert(key_int);
        EXPECT_EQ(r.second, true);
    }
    
    // Check that each generated key belongs to expected key space
    for(size_t i=0; i<1e3; ++i) {
        const char* key = gen->next(false);
        uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
        EXPECT_EQ(key_space.count(key_int), 1);
    }
}

TYPED_TEST(KeyGeneratorTest, Prefix)
{
    auto gen = this->Instantiate(10, 8, "user_");
    EXPECT_EQ(gen->size(), 13);

    const char* key = gen->next(false);
    EXPECT_EQ(memcmp(key, "user_", 5), 0);
}

TYPED_TEST(KeyGeneratorTest, LargeKey)
{
    auto gen = this->Instantiate(10, 16);
    EXPECT_EQ(gen->size(), 16);

    const char* key = gen->next(false);
    uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
    EXPECT_EQ(key_int, 0);
}

TYPED_TEST(KeyGeneratorTest, LargeKeyPrefix)
{
    auto gen = this->Instantiate(10, 16, "user_");
    EXPECT_EQ(gen->size(), 21);

    const char* key = gen->next(false);
    EXPECT_EQ(memcmp(key, "user_", 5), 0);
    key += 5; 
    uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
    EXPECT_EQ(key_int, 0);
}

TYPED_TEST(KeyGeneratorTest, SimpleMultithread)
{
    auto gen = this->Instantiate(10, 8);
    gen->set_seed(1729);
    
    std::unordered_set<uint64_t> key_space;
    std::thread other ([&gen, &key_space]()
        {
            EXPECT_EQ(gen->keyspace(), 10);
            EXPECT_EQ(gen->size(), 8);
            EXPECT_EQ(gen->get_seed(), 0);
            gen->set_seed(666);
            EXPECT_EQ(gen->get_seed(), 666);

            for(size_t i=1; i<=10; ++i) {
                const char* key = gen->next(true);
                uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
                EXPECT_EQ(key_int, PiBench::utils::multiplicative_hash<uint64_t>(i));
        
                auto r = key_space.insert(key_int);
                EXPECT_EQ(r.second, true);
            }

            // Check that each generated key belongs to expected key space
            for(size_t i=0; i<1e3; ++i) {
                const char* key = gen->next(false);
                uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
                EXPECT_EQ(key_space.count(key_int), 1);
            }
        }
    );

    other.join();
    EXPECT_EQ(gen->get_seed(), 1729);

    // Check that each generated key belongs to expected key space
    for(size_t i=0; i<1e3; ++i) {
        const char* key = gen->next(false);
        uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
        EXPECT_EQ(key_space.count(key_int), 1);
    }
}

TYPED_TEST(KeyGeneratorTest, ResetSeed)
{
    auto gen = this->Instantiate(10, 8);
    gen->set_seed(1729);
    uint64_t key1 = *reinterpret_cast<const uint64_t*>(gen->next(false));

    gen->set_seed(1729);
    uint64_t key2 = *reinterpret_cast<const uint64_t*>(gen->next(false));

    EXPECT_EQ(key1, key2);
}

}  // namespace

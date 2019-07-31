#ifndef __OPERATION_GENERATOR_HPP__
#define __OPERATION_GENERATOR_HPP__

#include <cstdint>
#include <random>

namespace PiBench
{

enum class operation_t : uint8_t
{
    READ,
    INSERT,
    UPDATE,
    REMOVE,
    SCAN,
    UNKNOWN
};

class operation_generator_t
{
public:
    /**
     * @brief Construct a new operation_generator_t object
     *
     * @param read ratio of read operations.
     * @param insert ratio of insert operations.
     * @param update ratio of update operations.
     * @param remove ratio of remove operations.
     * @param scan ratio of scan operations.
     */
    operation_generator_t(float read, float insert, float update, float remove, float scan)
        : dist_({read, insert, update, remove, scan})
    {
    }

    /**
     * @brief Returns next random operation.
     *
     * @return operation_t
     */
    operation_t next()
    {
        return static_cast<operation_t>(dist_(gen_));
    }

    /**
     * @brief Set the seed object.
     *
     * @param seed
     */
    static void set_seed(uint32_t seed)
    {
        seed_ = seed;
        gen_.seed(seed_);
    }

    /**
     * @brief Get the seed object.
     *
     * @return uint32_t
     */
    static uint32_t get_seed() noexcept { return seed_; }

private:
    /// Seed used for generating random numbers.
    static thread_local uint32_t seed_;

    /// Engine used for generation random numbers.
    static thread_local std::default_random_engine gen_;

    /// Weighted distribution for generating random numbers.
    std::discrete_distribution<uint32_t> dist_;
};
} // namespace PiBench

#endif

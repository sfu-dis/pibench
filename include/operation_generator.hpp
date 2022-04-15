#ifndef __OPERATION_GENERATOR_HPP__
#define __OPERATION_GENERATOR_HPP__

#include <cstdint>
#include <random>
#include <array>
#include "third_party/foedus/uniform_random.hpp"

namespace PiBench
{

enum class operation_t : uint8_t
{
    READ = 0,
    INSERT = 1,
    UPDATE = 2,
    REMOVE = 3,
    SCAN = 4
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
    {
        std::default_random_engine gen;
        std::discrete_distribution<uint32_t> op_weights({read, insert, update, remove, scan});

        for(unsigned int i=0; i<ops_.size(); ++i) {
            ops_[i] = static_cast<operation_t>(op_weights(gen));
        }
    }

    /**
     * @brief Returns next random operation.
     *
     * @return operation_t
     */
    operation_t next()
    {
        return ops_[rng_.next_uint32() & 0xff];
    }

    /**
     * @brief Set the seed object.
     *
     * @param seed
     */
    static void set_seed(uint32_t seed)
    {
        rng_.set_current_seed(seed);
    }

    /**
     * @brief Get the seed object.
     *
     * @return uint32_t
     */
    static uint32_t get_seed() noexcept { return rng_.get_current_seed(); }

private:
    /// Engine used for generation random numbers.
    static thread_local foedus::assorted::UniformRandom rng_;

    std::array<operation_t, 256> ops_;
};
} // namespace PiBench

#endif
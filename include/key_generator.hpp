#ifndef __KEY_GENERATOR_HPP__
#define __KEY_GENERATOR_HPP__

#include "rdtsc.hpp"
#include "selfsimilar_int_distribution.hpp"
#include "zipfian_int_distribution.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>

namespace PiBench
{

/**
 * @brief Class used to generate random keys of a given size belonging to a
 * keyspace of given size.
 *
 * The generated keys are composed of two parts:
 * |----- prefix (optional) -----||---- id -----|
 *
 * The generated 'ids' are 8 Byte unsigned integers. The 'ids' are then hashed
 * to scramble the keys across the keyspace.
 *
 * If the specified key size is smaller than 8 Bytes, the higher bits are
 * discarded. If the specified key size is larger than 8 Bytes, zeroes are
 * preppended to match the size.
 *
 */
class key_generator_t
{
public:
    /**
     * @brief Construct a new key_generator_t object
     *
     * @param N size of key space.
     * @param size size in Bytes of keys to be generated (excluding prefix).
     * @param apply_hash whether to apply the multiplicative hash function.
     * @param prefix prefix to be prepended to every key.
     */
    key_generator_t(size_t N, size_t size, bool apply_hash = true,
                    const std::string& prefix = "");

    virtual ~key_generator_t() = default;

    /**
     * @brief Generate next key.
     *
     * Setting 'in_sequence' to true is useful when initially loading the data
     * structure, so no repeated keys are generated and we can guarantee the
     * amount of unique records inserted.
     *
     * Finally, a pointer to buf_ is returned. Since next() overwrites buf_, the
     * pointer returned should not be used across calls to next().
     *
     * @param in_sequence if @true, keys are generated in sequence,
     *                    if @false keys are generated randomly.
     * @return const char* pointer to beginning of key.
     */
    virtual const char* next(bool in_sequence = false);

    /**
     * @brief Returns total key size (including prefix).
     *
     * @return size_t
     */
    size_t size() const noexcept { return prefix_.size() + size_; }

    /**
     * @brief Returns size of keyspace in number of elements..
     *
     * @return size_t
     */
    size_t keyspace() const noexcept { return N_; }

    /**
     * @brief Set the seed object.
     *
     * @param seed
     */
    static void set_seed(uint32_t seed)
    {
        seed_ = seed;
        generator_.seed(seed_);
    }

    /**
     * @brief Get the seed object.
     *
     * @return uint32_t
     */
    static uint32_t get_seed() noexcept { return seed_; }

    static constexpr uint32_t KEY_MAX = 128;

    static thread_local uint64_t current_id_;

    const char* hash_id(uint64_t id);

    virtual uint64_t next_id() = 0;

protected:

    /// Engine used for generating random numbers.
    static thread_local std::default_random_engine generator_;

private:
    /// Seed used for generating random numbers.
    static thread_local uint32_t seed_;

    /// Space to materialize the keys (avoid allocation).
    static thread_local char buf_[KEY_MAX];

    /// Size of keyspace to generate keys.
    const size_t N_;

    /// Size in Bytes of keys to be generated (excluding prefix).
    const size_t size_;

    // Whether to apply the multiplicative hash function.
    const bool apply_hash_;

    /// Prefix to be preppended to every key.
    const std::string prefix_;

    //uint64_t current_id_ = 0;
};

class uniform_key_generator_t final : public key_generator_t
{
public:
    uniform_key_generator_t(size_t N, size_t size, bool apply_hash = true,
                            const std::string& prefix = "")
        : dist_(0, N - 1),
          key_generator_t(N, size, apply_hash, prefix) {}

protected:
    virtual uint64_t next_id() override
    {
        return dist_(generator_);
    }

private:
    std::uniform_int_distribution<uint64_t> dist_;
};

class selfsimilar_key_generator_t final : public key_generator_t
{
public:
    selfsimilar_key_generator_t(size_t N, size_t size, bool apply_hash = true,
                                const std::string& prefix = "", float skew = 0.2)
        : dist_(0, N - 1, skew),
          key_generator_t(N, size, apply_hash, prefix)
    {
    }

    virtual uint64_t next_id() override
    {
        return dist_(generator_);
    }

private:
    selfsimilar_int_distribution<uint64_t> dist_;
};

class zipfian_key_generator_t final : public key_generator_t
{
public:
    zipfian_key_generator_t(size_t N, size_t size, bool apply_hash = true,
                            const std::string& prefix = "", float skew = 0.99)
        : dist_(0, N - 1, skew),
          key_generator_t(N, size, apply_hash, prefix)
    {
    }

    virtual uint64_t next_id() override
    {
        return dist_(generator_);
    }

private:
    zipfian_int_distribution<uint64_t> dist_;
};

class rdtsc_key_generator_t final : public key_generator_t
{
public:
    rdtsc_key_generator_t(size_t N, size_t size, bool apply_hash = true,
                          const std::string& prefix = "")
        : key_generator_t(N, size, apply_hash, prefix) {}

    // Ignore 'in_sequence' and always use RDTSC.
    virtual const char* next(bool in_sequence = false) override;

protected:
    virtual uint64_t next_id() override
    {
        return rdtsc();
    }
};
} // namespace PiBench
#endif

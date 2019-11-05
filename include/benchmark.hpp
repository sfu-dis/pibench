#ifndef __NVM_TREE_BENCH_HPP__
#define __NVM_TREE_BENCH_HPP__

#include "cpucounters.h"
#include "key_generator.hpp"
#include "operation_generator.hpp"
#include "stopwatch.hpp"
#include "tree_api.hpp"
#include "value_generator.hpp"

#include <cstdint>
#include <memory> // For unique_ptr
#include <chrono> // std::chrono::high_resolution_clock::time_point
#include <vector>

namespace PiBench
{

/**
 * @brief Supported random number distributions.
 *
 */
enum class distribution_t : uint8_t
{
    UNIFORM,
    SELFSIMILAR,
    ZIPFIAN
};

/**
 * @brief Benchmark options.
 *
 */
struct options_t
{
    /// Name of tree library file used to run the benchmark against.
    std::string library_file = "";

    /// Number of records to insert into tree during 'load' phase.
    uint64_t num_records;

    /// Number of operations to issue during 'run' phase.
    uint64_t num_ops;

    /// Number of parallel threads used for executing requests.
    uint32_t num_threads;

    /// Sampling window in milliseconds.
    uint32_t sampling_ms;

    /// Key prefix.
    std::string key_prefix = "";

    /// Size of key in bytes.
    uint32_t key_size;

    /// Size of value in bytes.
    uint32_t value_size;

    /// Ratio of read operations.
    float read_ratio;

    /// Ratio of insert operations.
    float insert_ratio;

    /// Ratio of update operations.
    float update_ratio;

    /// Ratio of remove operations.
    float remove_ratio;

    /// Ratio of scan operations.
    float scan_ratio;

    /// Size of scan operations in records.
    uint32_t scan_size;

    /// Distribution used for generation random keys.
    distribution_t key_distribution;

    /// Factor to be used for skewed random key distributions.
    float key_skew;

    /// Master seed to be used for random generations.
    uint32_t rnd_seed;

    /// Whether to enable Intel PCM for profiling.
    bool enable_pcm;

    /// Whether to skip the load phase.
    bool skip_load;

    /// Ratio of requests to sample latency from (between 0.0 and 1.0).
    float latency_sampling;
};

/**
 * @brief Statistics collected over time.
 *
 */
struct alignas(64) stats_t
{
    stats_t()
        : operation_count(0)
    {
    }

    /// Number of operations completed.
    uint64_t operation_count;

    /// Vector to store both start and end time of requests.
    std::vector<std::chrono::high_resolution_clock::time_point> times;

    /// Padding to enforce cache-line size and avoid cache-line ping-pong.
    uint64_t ____padding[7];
};

class benchmark_t
{
public:
    /**
     * @brief Construct a new benchmark_t object.
     *
     * @param tree pointer to tree data structure compliant with the API.
     * @param opt options used to run the benchmark.
     */
    benchmark_t(tree_api* tree, const options_t& opt) noexcept;

    /**
     * @brief Destroy the benchmark_t object.
     *
     */
    ~benchmark_t();

    /**
     * @brief Load the tree with the amount of records specified in options_t.
     *
     * A single thread is used for load phase to guarantee determinism across
     * multiple runs. Using multiple threads can lead to different, but
     * equivalent, results.
     */
    void load() noexcept;

    /// Run the workload as specified by options_t.
    void run() noexcept;

    /// Maximum number of records to be scanned.
    static constexpr size_t MAX_SCAN = 1000;

private:
    /// Tree data structure being benchmarked.
    tree_api* tree_;

    /// Options used to run this benchmark.
    const options_t opt_;

    /// Operation generator.
    operation_generator_t op_generator_;

    /// Key generator.
    std::unique_ptr<key_generator_t> key_generator_;

    /// Value generator.
    value_generator_t value_generator_;

    /// Intel PCM handler.
    PCM* pcm_;
};
} // namespace PiBench

namespace std
{
std::ostream& operator<<(std::ostream& os, const PiBench::distribution_t& dist);
std::ostream& operator<<(std::ostream& os, const PiBench::options_t& opt);
} // namespace std

#endif

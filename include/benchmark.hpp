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

#if defined(EPOCH_BASED_RECLAMATION)
#include "third_party/art_ebr/Epoche.h"
#endif

namespace PiBench
{

void print_environment();

/**
 * @brief Benchmark mode
 */
enum class mode_t : uint8_t
{
    Operation = 0,
    Time = 1,
};

/**
 * @brief Supported random number distributions.
 *
 */
enum class distribution_t : uint8_t
{
    UNIFORM = 0,
    SELFSIMILAR = 1,
    ZIPFIAN = 2,
    RDTSC = 3
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
    uint64_t num_records = 1e6;

    /// Number of operations to issue during 'run' phase.
    uint64_t num_ops = 1e6;

    /// Number of parallel threads used for executing requests.
    uint32_t num_threads = 1;

    /// Sampling window in milliseconds.
    uint32_t sampling_ms = 1000;

    /// Key prefix.
    std::string key_prefix = "";

    /// Size of key in bytes.
    uint32_t key_size = 8;

    /// Size of value in bytes.
    uint32_t value_size = 8;

    /// Ratio of read operations.
    float read_ratio = 1.0;

    /// Ratio of insert operations.
    float insert_ratio = 0.0;

    /// Ratio of update operations.
    float update_ratio = 0.0;

    /// Ratio of remove operations.
    float remove_ratio = 0.0;

    /// Ratio of scan operations.
    float scan_ratio = 0.0;

    /// Size of scan operations in records.
    uint32_t scan_size = 100;

    /// Distribution used for generation random keys.
    distribution_t key_distribution = distribution_t::UNIFORM;

    /// Factor to be used for skewed random key distributions.
    float key_skew = 0.2;

    /// Master seed to be used for random generations.
    uint32_t rnd_seed = 1729;

    /// Whether to enable Intel PCM for profiling.
    bool enable_pcm = true;

    /// Whether to do bulk loading.
    bool bulk_load = false;

    /// Whether to skip the load phase.
    bool skip_load = false;

    /// Whether to skip the verify phase.
    bool skip_verify = false;

    /// Whether to apply the multiplicative hash function.
    bool apply_hash = true;

    /// Ratio of requests to sample latency from (between 0.0 and 1.0).
    float latency_sampling = 0.0;

    /// Experiment duration (seconds) for time-based benchmark
    uint32_t seconds = 20;

    /// Experiment mode
    mode_t bm_mode = mode_t::Operation;

    /// Number of operations before exiting/re-entering epochs
    uint32_t epoch_ops_threshold = 1024;

    /// Number of deletions before performing garbage collection
    uint32_t epoch_gc_threshold = 256;

    /// Whether to use perf for profiling.
    bool enable_perf = false;

    /// Arguments to perf-record
    std::string perf_record_args = "";
};

/**
 * @brief Statistics collected over time.
 *
 */
struct alignas(64) stats_t
{
    stats_t()
        : operation_count(0)
        , insert_count(0)
        , success_insert_count(0)
        , read_count(0)
        , success_read_count(0)
        , update_count(0)
        , success_update_count(0)
        , remove_count(0)
        , success_remove_count(0)
        , scan_count(0)
        , success_scan_count(0)
    {
    }

    /// Number of operations completed.
    uint64_t operation_count;

    /// Number of inserts completed
    uint64_t insert_count;

    /// Number of successful inserts
    uint64_t success_insert_count;

    /// Number of reads completed
    uint64_t read_count;

    /// Number of successful point reads
    uint64_t success_read_count;

    /// Number of updates completed
    uint64_t update_count;

    /// Number of successful point reads
    uint64_t success_update_count;

    /// Number of remove completed
    uint64_t remove_count;

    /// Number of successful point reads
    uint64_t success_remove_count;

    /// Number of scan completed
    uint64_t scan_count;

    /// Number of successful point reads
    uint64_t success_scan_count;

    /// Vector to store both start and end time of requests.
    std::vector<std::chrono::high_resolution_clock::time_point> times;
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
    void run_op(operation_t op, const char *key_ptr, 
                char *value_out, char *values_out, bool measure_latency,
                stats_t &stats);

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

#if defined(EPOCH_BASED_RECLAMATION)
    /// Epoch manager
    ART::Epoche epoch_;

public:
    ART::Epoche& getEpoch() { return epoch_; }

    void setTree(tree_api *tree) { tree_ = tree; }

private:
#endif
};
} // namespace PiBench

namespace std
{
std::ostream& operator<<(std::ostream& os, const PiBench::distribution_t& dist);
std::ostream& operator<<(std::ostream& os, const PiBench::options_t& opt);
} // namespace std

#endif

#include "benchmark.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <omp.h>
#include <functional> // std::bind
#include <cmath>      // std::ceil

namespace PiBench
{

benchmark_t::benchmark_t(tree_api* tree, const options_t& opt) noexcept
    : tree_(tree),
      opt_(opt),
      op_generator_(opt.read_ratio, opt.insert_ratio, opt.update_ratio, opt.remove_ratio, opt.scan_ratio),
      value_generator_(opt.value_size),
      pcm_(nullptr)
{
    if (opt.enable_pcm)
    {
        pcm_ = PCM::getInstance();
        auto status = pcm_->program();
        if (status != PCM::Success)
        {
            std::cout << "Error opening PCM: " << status << std::endl;
            if (status == PCM::PMUBusy)
                pcm_->resetPMU();
            else
                exit(0);
        }
    }

    size_t key_space_sz = opt_.num_records + (opt_.num_ops * opt_.insert_ratio);
    switch (opt_.key_distribution)
    {
    case distribution_t::UNIFORM:
        key_generator_ = std::make_unique<uniform_key_generator_t>(key_space_sz, opt_.key_size, opt_.key_prefix);
        break;

    case distribution_t::SELFSIMILAR:
        key_generator_ = std::make_unique<selfsimilar_key_generator_t>(key_space_sz, opt_.key_skew, opt_.key_size, opt_.key_prefix);
        break;

    case distribution_t::ZIPFIAN:
        key_generator_ = std::make_unique<zipfian_key_generator_t>(key_space_sz, opt_.key_skew, opt_.key_size, opt_.key_prefix);
        break;

    default:
        std::cout << "Error: unknown distribution!" << std::endl;
        exit(0);
    }
}

benchmark_t::~benchmark_t()
{
    if (pcm_)
        pcm_->cleanup();
}

void benchmark_t::load() noexcept
{
    if(opt_.skip_load)
    {
        key_generator_->current_id_ = opt_.num_records + 1;
        return;
    }

    stopwatch_t sw;
    sw.start();
    for (uint32_t i = 0; i < opt_.num_records; ++i)
    {
        // Generate key in sequence
        auto key_ptr = key_generator_->next(true);

        // Generate random value
        auto value_ptr = value_generator_.next();

        auto r = tree_->insert(key_ptr, key_generator_->size(), value_ptr, opt_.value_size);
        assert(r);
    }
    auto elapsed = sw.elapsed<std::chrono::milliseconds>();

    std::cout << "Overview:"
              << "\n"
              << "\tLoad time: " << elapsed << " milliseconds" << std::endl;
}

void benchmark_t::run() noexcept
{
    std::vector<stats_t> global_stats;
    global_stats.reserve(100000); // Avoid overhead of resizing vector

    static thread_local char value_out[value_generator_t::VALUE_MAX];
    char* values_out;

    std::vector<stats_t> local_stats(opt_.num_threads);
    for(auto& lc : local_stats)
        lc.times.reserve(std::ceil(opt_.num_ops/opt_.num_threads)*2);

    // Control variable of monitor thread
    bool finished = false;

    // The amount of inserts expected to be done by each thread + some play room.
    uint64_t inserts_per_thread = 10 + (opt_.num_ops * opt_.insert_ratio) / opt_.num_threads;

    // Current id after load
    uint64_t current_id = key_generator_->current_id_;

    std::unique_ptr<SystemCounterState> before_sstate;
    if (opt_.enable_pcm)
    {
        before_sstate = std::make_unique<SystemCounterState>();
        *before_sstate = getSystemCounterState();
    }

    float elapsed = 0.0;
    stopwatch_t sw;
    omp_set_nested(true);
    #pragma omp parallel sections num_threads(2)
    {
        #pragma omp section // Monitor thread
        {
            std::chrono::milliseconds sampling_window(opt_.sampling_ms);
            while (!finished)
            {
                std::this_thread::sleep_for(sampling_window);
                stats_t s;
                s.operation_count = std::accumulate(local_stats.begin(), local_stats.end(), 0,
                                                    [](uint64_t sum, const stats_t& curr) {
                                                        return sum + curr.operation_count;
                                                    });
                global_stats.push_back(std::move(s));
            }
        }

        #pragma omp section // Worker threads
        {
            #pragma omp parallel num_threads(opt_.num_threads)
            {
                auto tid = omp_get_thread_num();

                // Initialize random seed for each thread
                key_generator_->set_seed(opt_.rnd_seed * (tid + 1));

                // Initialize insert id for each thread
                key_generator_->current_id_ = current_id + (inserts_per_thread * tid);

                auto random_bool = std::bind(std::bernoulli_distribution(opt_.latency_sampling), std::knuth_b());

                #pragma omp barrier

                #pragma omp single nowait
                {
                    sw.start();
                }

                #pragma omp for schedule(static)
                for (uint64_t i = 0; i < opt_.num_ops; ++i)
                {
                    // Generate random operation
                    auto op = op_generator_.next();

                    // Generate random scrambled key
                    auto key_ptr = key_generator_->next(op == operation_t::INSERT ? true : false);

                    auto measure_latency = random_bool();
                    if(measure_latency)
                    {
                        local_stats[tid].times.push_back(std::chrono::high_resolution_clock::now());
                    }

                    switch (op)
                    {
                    case operation_t::READ:
                    {
                        auto r = tree_->find(key_ptr, key_generator_->size(), value_out);
                        assert(r);
                        break;
                    }

                    case operation_t::INSERT:
                    {
                        // Generate random value
                        auto value_ptr = value_generator_.next();
                        auto r = tree_->insert(key_ptr, key_generator_->size(), value_ptr, opt_.value_size);
                        assert(r);
                        break;
                    }

                    case operation_t::UPDATE:
                    {
                        // Generate random value
                        auto value_ptr = value_generator_.next();
                        auto r = tree_->update(key_ptr, key_generator_->size(), value_ptr, opt_.value_size);
                        assert(r);
                        break;
                    }

                    case operation_t::REMOVE:
                    {
                        auto r = tree_->remove(key_ptr, key_generator_->size());
                        assert(r);
                        break;
                    }

                    case operation_t::SCAN:
                    {
                        auto r = tree_->scan(key_ptr, key_generator_->size(), opt_.scan_size, values_out);
                        assert(r);
                        break;
                    }

                    default:
                        std::cout << "Error: unknown operation!" << std::endl;
                        exit(0);
                        break;
                    }

                    if(measure_latency)
                    {
                        local_stats[tid].times.push_back(std::chrono::high_resolution_clock::now());
                    }
                    ++local_stats[tid].operation_count;
                }

                // Get elapsed time and signal monitor thread to finish.
                #pragma omp single nowait
                {
                    elapsed = sw.elapsed<std::chrono::milliseconds>();
                    finished = true;
                }
            }
        }
    }
    omp_set_nested(false);

    std::unique_ptr<SystemCounterState> after_sstate;
    if (opt_.enable_pcm)
    {
        after_sstate = std::make_unique<SystemCounterState>();
        *after_sstate = getSystemCounterState();
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\tRun time: " << elapsed << " milliseconds" << std::endl;
    std::cout << "\tThroughput: " << opt_.num_ops / ((float)elapsed / 1000)
              << " ops/s" << std::endl;

    if (opt_.enable_pcm)
    {
        std::cout << "PCM Metrics:"
                  << "\n"
                  << "\tL3 misses: " << getL3CacheMisses(*before_sstate, *after_sstate) << "\n"
                  << "\tDRAM Reads (bytes): " << getBytesReadFromMC(*before_sstate, *after_sstate) << "\n"
                  << "\tDRAM Writes (bytes): " << getBytesWrittenToMC(*before_sstate, *after_sstate) << "\n"
                  << "\tNVM Reads (bytes): " << getBytesReadFromPMM(*before_sstate, *after_sstate) << "\n"
                  << "\tNVM Writes (bytes): " << getBytesWrittenToPMM(*before_sstate, *after_sstate) << std::endl;
    }

    std::cout << "Samples:" << std::endl;
    std::adjacent_difference(global_stats.begin(), global_stats.end(), global_stats.begin(),
                             [](const stats_t& x, const stats_t& y) {
                                 stats_t s;
                                 s.operation_count = x.operation_count - y.operation_count;
                                 return s;
                             });

    for (auto s : global_stats)
        std::cout << "\t" << s.operation_count << std::endl;

    if(opt_.latency_sampling > 0.0)
    {
        std::vector<uint64_t> global_latencies;
        for(auto& v : local_stats)
            for(unsigned int i=0; i<v.times.size(); i=i+2)
                global_latencies.push_back(std::chrono::nanoseconds(v.times[i+1]-v.times[i]).count());

        std::sort(global_latencies.begin(), global_latencies.end());
        auto observed = global_latencies.size();
        std::cout << "Latencies (" << observed << " operations observed):\n"
                  << "\tmin: " << global_latencies[0] << '\n'
                  << "\t50%: " << global_latencies[0.5*observed] << '\n'
                  << "\t90%: " << global_latencies[0.9*observed] << '\n'
                  << "\t99%: " << global_latencies[0.99*observed] << '\n'
                  << "\t99.9%: " << global_latencies[0.999*observed] << '\n'
                  << "\t99.99%: " << global_latencies[0.9999*observed] << '\n'
                  << "\t99.999%: " << global_latencies[0.99999*observed] << '\n'
                  << "\tmax: " << global_latencies[observed-1] << std::endl;
    }
}
} // namespace PiBench

namespace std
{
std::ostream& operator<<(std::ostream& os, const PiBench::distribution_t& dist)
{
    switch (dist)
    {
    case PiBench::distribution_t::UNIFORM:
        return os << "UNIFORM";
        break;
    case PiBench::distribution_t::SELFSIMILAR:
        return os << "SELFSIMILAR";
        break;
    case PiBench::distribution_t::ZIPFIAN:
        return os << "ZIPFIAN";
        break;
    default:
        return os << static_cast<uint8_t>(dist);
    }
}

std::ostream& operator<<(std::ostream& os, const PiBench::options_t& opt)
{
    os << "Benchmark Options:"
       << "\n"
       << "\tTarget: " << opt.library_file << "\n"
       << "\t# Records: " << opt.num_records << "\n"
       << "\t# Operations: " << opt.num_ops << "\n"
       << "\t# Threads: " << opt.num_threads << "\n"
       << "\tSampling: " << opt.sampling_ms << " ms\n"
       << "\tLatency: " << opt.latency_sampling << "\n"
       << "\tKey prefix: " << opt.key_prefix << "\n"
       << "\tKey size: " << opt.key_size << "\n"
       << "\tValue size: " << opt.value_size << "\n"
       << "\tRandom seed: " << opt.rnd_seed << "\n"
       << "\tKey distribution: " << opt.key_distribution
       << (opt.key_distribution == PiBench::distribution_t::SELFSIMILAR || opt.key_distribution == PiBench::distribution_t::ZIPFIAN
               ? "(" + std::to_string(opt.key_skew) + ")"
               : "")
       << "\n"
       << "\tScan size: " << opt.scan_size << "\n"
       << "\tOperations ratio:\n"
       << "\t\tRead: " << opt.read_ratio << "\n"
       << "\t\tInsert: " << opt.insert_ratio << "\n"
       << "\t\tUpdate: " << opt.update_ratio << "\n"
       << "\t\tDelete: " << opt.remove_ratio << "\n"
       << "\t\tScan: " << opt.scan_ratio;
    return os;
}
} // namespace std
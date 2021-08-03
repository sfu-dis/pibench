#include "benchmark.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <omp.h>
#include <functional> // std::bind
#include <cmath>      // std::ceil
#include <ctime>
#include <fstream>
#include <regex>            // std::regex_replace
#include <sys/utsname.h>    // uname
#include <atomic> // std::atomic<T>

namespace PiBench
{

void print_environment()
{
    std::time_t now = std::time(nullptr);
    uint64_t num_cpus = 0;
    std::string cpu_type;
    std::string cache_size;

    std::ifstream cpuinfo("/proc/cpuinfo", std::ifstream::in);

    if(!cpuinfo.good())
    {
        num_cpus = 0;
        cpu_type = "Could not open /proc/cpuinfo";
        cache_size = "Could not open /proc/cpuinfo";
    }
    else
    {
        std::string line;
        while(!getline(cpuinfo, line).eof())
        {
            auto sep_pos = line.find(':');
            if(sep_pos == std::string::npos)
            {
                continue;
            }
            else
            {
                std::string key = std::regex_replace(std::string(line, 0, sep_pos), std::regex("\\t+$"), "");
                std::string val = sep_pos == line.size()-1 ? "" : std::string(line, sep_pos+2, line.size());
                if(key.compare("model name") == 0)
                {
                    ++num_cpus;
                    cpu_type = val;
                }
                else if(key.compare("cache size") == 0)
                {
                    cache_size = val;
                }
            }
        }
    }
    cpuinfo.close();

    std::string kernel_version;
    struct utsname uname_buf;
    if(uname(&uname_buf) == -1)
    {
        kernel_version = "Unknown";
    }
    else
    {
        kernel_version = std::string(uname_buf.sysname) + " " + std::string(uname_buf.release);
    }

    std::cout << "Environment:" << "\n"
              << "\tTime: " << std::asctime(std::localtime(&now))
              << "\tCPU: " << num_cpus << " * " << cpu_type << "\n"
              << "\tCPU Cache: " << cache_size << "\n"
              << "\tKernel: " << kernel_version << std::endl;
}

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
            key_generator_ = std::make_unique<uniform_key_generator_t>(key_space_sz, opt_.key_size, opt_.num_threads, opt_.bm_mode == mode_t::Operation ? false : true, opt_.key_prefix);
            break;

        case distribution_t::SELFSIMILAR:
            key_generator_ = std::make_unique<selfsimilar_key_generator_t>(key_space_sz, opt_.key_size, opt_.num_threads, opt_.bm_mode == mode_t::Operation ? false : true, opt_.key_prefix, opt_.key_skew);
            break;

        case distribution_t::ZIPFIAN:
            key_generator_ = std::make_unique<zipfian_key_generator_t>(key_space_sz, opt_.key_size, opt_.num_threads, opt_.bm_mode == mode_t::Operation ? false : true, opt_.key_prefix, opt_.key_skew);
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
    uint64_t insert_per_thread = opt_.num_records / opt_.num_threads;

    if(opt_.skip_load)
    {
        if(opt_.bm_mode == mode_t::Operation)
        {
            key_generator_->current_id_ = opt_.num_records + 1;
        }
        else if(opt_.bm_mode == mode_t::Time)
        {
            for(uint16_t i =0; i <opt_.num_threads; i++)
            {
                if(i!=opt_.num_threads-1)
                    key_generator_->thread_stat[i] = insert_per_thread + 1;
                else
                    key_generator_->thread_stat[i] = opt_.num_records % opt_.num_threads + insert_per_thread + 1;
            }
        }
        return;
    }

    /// Generating tid prefix when running time based benchmark
    auto tid_generate = [insert_per_thread](uint64_t i, uint64_t num_threads){
        if(i < insert_per_thread * num_threads){
            return i / insert_per_thread;
        }
        else
        {
            return num_threads - 1;
        }
    };

    stopwatch_t sw;
    sw.start();
    for (uint64_t i = 0; i < opt_.num_records; ++i)
    {
        // Generate key in sequence
        auto key_ptr = opt_.bm_mode == mode_t::Operation ? key_generator_->next(true) : key_generator_->next(tid_generate(i,opt_.num_threads), 0, true);

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

    std::vector<stats_t> local_stats(opt_.num_threads);

    global_stats.resize(
            opt_.bm_mode == mode_t::Operation ? 100000 :
                                                        static_cast<uint64_t>((opt_.time * 1000 / opt_.sampling_ms) + 10)); // Avoid overhead of allocation and page fault
    global_stats.resize(0);

    if(opt_.bm_mode == mode_t::Operation)
    {
        for(auto& lc : local_stats)
        {
            lc.times.resize(std::ceil(opt_.num_ops / opt_.num_threads * 2 ));
            lc.times.resize(0);
        }
    }

    else
    {
        for(auto& lc : local_stats)
        {
            lc.times.resize(std::ceil(static_cast<uint64_t>(100000 * opt_.num_threads *  opt_.time)));
            lc.times.resize(0);
        }
    }


    static thread_local char value_out[value_generator_t::VALUE_MAX];
    char* values_out;

    // Control variable of monitor thread
    std::atomic<bool> finished(false);

    std::unique_ptr<SystemCounterState> before_sstate;
    if (opt_.enable_pcm)
    {
        before_sstate = std::make_unique<SystemCounterState>();
        *before_sstate = getSystemCounterState();
    }

    stopwatch_t stopwatch;
    float elapsed = 0.0;

    // Start Benchmark
    // Operation based mode
    if(opt_.bm_mode == mode_t::Operation)
    {


        // The amount of inserts expected to be done by each thread + some play room.
        uint64_t inserts_per_thread = 10 + (opt_.num_ops * opt_.insert_ratio) / opt_.num_threads;

        // Current id after load
        uint64_t current_id = key_generator_->current_id_;

        omp_set_nested(true);
        #pragma omp parallel sections num_threads(2)
        {
            #pragma omp section // Monitor thread
            {
                std::chrono::milliseconds sampling_window(opt_.sampling_ms);
                while (!finished.load())
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
                        stopwatch.start();
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

                        if(!run_op(op,key_ptr,value_out,values_out))
                            ++local_stats[tid].operation_count_F;

                        if(measure_latency)
                        {
                            local_stats[tid].times.push_back(std::chrono::high_resolution_clock::now());
                        }
                        ++local_stats[tid].operation_count;
                    }

                    // Get elapsed time and signal monitor thread to finish.
                    #pragma omp single nowait
                    {
                        elapsed = stopwatch.elapsed<std::chrono::milliseconds>();
                        finished.store(true);
                    }
                }
            }
        }
        omp_set_nested(false);

    }
    // Time based mode
    else
    {

        omp_set_nested(true);
        #pragma omp parallel sections num_threads(2) default(none) shared(finished,local_stats,global_stats,elapsed,values_out,std::cout,stopwatch)
        {
            #pragma omp section // Monitor & timer thread
            {
                std::chrono::milliseconds sampling_window(opt_.sampling_ms);
                while (!finished.load())
                {
                    std::this_thread::sleep_for(sampling_window);
                    stats_t s;
                    s.operation_count = std::accumulate(local_stats.begin(), local_stats.end(), 0,
                                                        [](uint64_t sum, const stats_t& curr) {
                                                            return sum + curr.operation_count;
                                                        });
                    global_stats.push_back(std::move(s));
                    if(stopwatch.elapsed<std::chrono::seconds>() > opt_.time)
                    {
                        finished.store(true);
                        elapsed = stopwatch.elapsed<std::chrono::milliseconds>();
                    }
                }
            }


            #pragma omp section // Worker threads
            {
                #pragma omp parallel num_threads(opt_.num_threads) shared(finished)
                {
                    auto tid = omp_get_thread_num();

                    key_generator_->set_seed(opt_.rnd_seed * (tid + 1));

                    // How many inserts are within this thread ID's range
                    key_generator_->current_id_ = key_generator_->thread_stat[tid];

                    auto random_bool = std::bind(std::bernoulli_distribution(opt_.latency_sampling), std::knuth_b());

                    #pragma omp barrier

                    #pragma single nowait
                    {
                        stopwatch.start();
                    }

                    while(!finished.load())
                    {

                        // Generate random operation
                        auto op = op_generator_.next();

                        decltype(key_generator_->next()) key_ptr;

                        // Generate random scrambled key
                        if(op == operation_t::INSERT)
                            key_ptr = key_generator_->next(tid, key_generator_->current_id_, true);
                        else if(op == operation_t::READ || op == operation_t::UPDATE)
                            // Generate some unrepeated key for READ & UPDATE (if negative_access == true)
                            // TODO: Make sure to generate a negative access key(dividing next() to two parts)
                            key_ptr = key_generator_->next(tid, opt_.negative_access ? key_generator_->thread_stat[tid] * 1.2 : key_generator_->thread_stat[tid] - 1, false);
                        else
                            key_ptr = key_generator_->next(tid, key_generator_->thread_stat[tid]-1, false);

                        auto measure_latency = random_bool();

                        if(measure_latency)
                        {
                            local_stats[tid].times.push_back(std::chrono::high_resolution_clock::now());
                        }

                        if(!run_op(op,key_ptr,value_out,values_out))
                            ++local_stats[tid].operation_count_F;

                        if(measure_latency)
                        {
                            local_stats[tid].times.push_back(std::chrono::high_resolution_clock::now());
                        }
                        ++local_stats[tid].operation_count;
                    }
                }

            }
        }
        omp_set_nested(false);
    }

    std::unique_ptr<SystemCounterState> after_sstate;
    if (opt_.enable_pcm)
    {
        after_sstate = std::make_unique<SystemCounterState>();    /// Storing the number of inserts with different thread ID
        *after_sstate = getSystemCounterState();
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\tRun time: " << elapsed << " milliseconds" << std::endl;

    // False operation number
    uint64_t op_num_f = 0;
    for(auto &lc: local_stats)
        op_num_f+=lc.operation_count_F;

    if(opt_.bm_mode == mode_t::Operation)
    {
        std::cout << "\tThroughput: " << opt_.num_ops / ((float)elapsed / 1000)
                  << " ops/s" << std::endl;
        std::cout << "\tFalse access rate: " << (float)op_num_f * 100.0 / opt_.num_ops << "%" <<std::endl;
    }
    else
    {
        // Number of operations done while benchmarking
        uint64_t op_num = 0;
        for(auto &lc: local_stats)
            op_num += lc.operation_count;
        std::cout << "\tThroughput: " << op_num / ((float)elapsed / 1000)
                  << " ops/s" << std::endl;
        std::cout << "\tFalse access rate: " << ((float)op_num_f * 100.0 / op_num) << "%" <<std::endl;
    }
 
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

bool benchmark_t::run_op(operation_t operation, const char *key_ptr, char *value_out, char *values_out)
{
    bool r;
    switch (operation)
    {
        case operation_t::READ:
        {
            r = tree_->find(key_ptr, key_generator_->size(), value_out);
            assert(r);
            break;
        }

        case operation_t::INSERT:
        {
            // Generate random value
            auto value_ptr = value_generator_.next();
            r = tree_->insert(key_ptr, key_generator_->size(), value_ptr, opt_.value_size);
            assert(r);
            break;
        }

        case operation_t::UPDATE:
        {
            // Generate random value
            auto value_ptr = value_generator_.next();
            r = tree_->update(key_ptr, key_generator_->size(), value_ptr, opt_.value_size);
            assert(r);
            break;
        }

        case operation_t::REMOVE:
        {
            // TODO: Randomly deleting keys will cause false read,update and scan
            r = tree_->remove(key_ptr, key_generator_->size());
            assert(r);
            break;
        }

        case operation_t::SCAN:
        {
            r = static_cast<bool>(tree_->scan(key_ptr, key_generator_->size(), opt_.scan_size, values_out));
            assert(r);
            break;
        }

        default:
            std::cout << "Error: unknown operation!" << std::endl;
            exit(0);
            break;
    }
    return r;
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
       << (opt.bm_mode == PiBench::mode_t::Operation ? "\t# Operations: " : "\t# Running time: ") << (opt.bm_mode == PiBench::mode_t::Operation ? opt.num_ops : opt.time) << "\n"
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
       << "\t\tScan: " << opt.scan_ratio << "\n"
       << "\t\tFalse access: " << std::boolalpha << opt.negative_access;
    return os;
}
} // namespace std
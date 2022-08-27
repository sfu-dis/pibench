#include "benchmark.hpp"
#include "utils.hpp"
#include "sched.hpp"
#include "third_party/foedus/bernoulli_random.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <omp.h>
#include <functional> // std::bind
#include <iterator>
#include <cmath>      // std::ceil
#include <ctime>
#include <fstream>
#include <regex>            // std::regex_replace
#include <sys/utsname.h>    // uname
#include <sys/wait.h>       // waitpid

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
#if defined(EPOCH_BASED_RECLAMATION)
      , epoch_(opt.epoch_gc_threshold)
#endif
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
        key_generator_ = std::make_unique<uniform_key_generator_t>(key_space_sz, opt_.key_size, opt_.apply_hash, opt_.key_prefix);
        break;

    case distribution_t::SELFSIMILAR:
        key_generator_ = std::make_unique<selfsimilar_key_generator_t>(key_space_sz, opt_.key_size, opt_.apply_hash, opt_.key_prefix, opt_.key_skew);
        break;

    case distribution_t::ZIPFIAN:
        key_generator_ = std::make_unique<zipfian_key_generator_t>(key_space_sz, opt_.key_size, opt_.apply_hash, opt_.key_prefix, opt_.key_skew);
        break;

    case distribution_t::RDTSC:
        key_generator_ = std::make_unique<rdtsc_key_generator_t>(key_space_sz, opt_.key_size, opt_.apply_hash, opt_.key_prefix);
        break;

    case distribution_t::PSEUDOSELFSIMILAR:
        key_generator_ = std::make_unique<pseudo_selfsimilar_key_generator_t>(key_space_sz, opt_.key_size, 64, opt_.num_threads, opt_.apply_hash, opt_.key_prefix, opt_.key_skew);
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
        std::cout << "Load skipped." << std::endl;
        key_generator_->current_id_ = opt_.num_records + 1;
        return;
    }

    std::cout << "Loading started." << std::endl;
    stopwatch_t sw;
    sw.start();

    if (opt_.bulk_load)
    {
        std::cout << "Bulk loading..." << std::endl;
        size_t num_bytes = opt_.num_records * (key_generator_->size() + opt_.value_size);
        char *kv_pairs = (char *)aligned_alloc(64, num_bytes);
        char *pos = kv_pairs;

        key_generator_->current_id_ = opt_.num_records / opt_.num_threads * omp_get_thread_num();
        for (uint64_t i = 0; i < opt_.num_records; ++i)
        {
            // Generate key in sequence
            auto key_ptr = key_generator_->next(true);

            // Generate random value
            auto value_ptr = value_generator_.next();

            memcpy(pos, key_ptr, key_generator_->size());
            pos += key_generator_->size();

            memcpy(pos, value_ptr, opt_.value_size);
            pos += opt_.value_size;
        }

        auto r = tree_->bulk_load(kv_pairs, opt_.num_records, key_generator_->size(), opt_.value_size);
        free(kv_pairs);

        if (!r)
        {
            std::cout << "Bulk loading failed!" << std::endl;
            exit(1);
        }
    }
    else
    {
        #pragma omp parallel num_threads(opt_.num_threads)
        {
            set_affinity(omp_get_thread_num());
            // Initialize insert id for each thread
            key_generator_->current_id_ = opt_.num_records / opt_.num_threads * omp_get_thread_num();

            auto pseudo_selfsimilar = dynamic_cast<pseudo_selfsimilar_key_generator_t *>(key_generator_.get());
            if (pseudo_selfsimilar)
            {
                pseudo_selfsimilar->set_start(omp_get_thread_num());
            }

#if defined(EPOCH_BASED_RECLAMATION)
            ART::ThreadInfo t(epoch_);
            uint32_t epoch_ops_threshold_count = 0;
            epoch_.enterEpoche(t);
#endif
            #pragma omp for schedule(static)
            for (uint64_t i = 0; i < opt_.num_records; ++i)
            {
                // Generate key in sequence
                auto key_ptr = key_generator_->next(true);

                // Generate random value
                auto value_ptr = value_generator_.next();

                auto r = tree_->insert(key_ptr, key_generator_->size(), value_ptr, opt_.value_size);
                assert(r);

#if defined(EPOCH_BASED_RECLAMATION)
                if (++epoch_ops_threshold_count == opt_.epoch_ops_threshold) {
                    epoch_.exitEpocheAndCleanup(t);
                    epoch_.enterEpoche(t);
                    epoch_ops_threshold_count = 0;
                }
#endif
            }
#if defined(EPOCH_BASED_RECLAMATION)
            epoch_.exitEpocheAndCleanup(t);
#endif
        }

    }

    auto elapsed = sw.elapsed<std::chrono::milliseconds>();

    std::cout << "Loading finished in " << elapsed << " milliseconds" << std::endl;

    if (opt_.skip_verify)
    {
        std::cout << "Verification skipped; benchmark started." << std::endl;
        return;
    }

    // Verify all keys can be found
    {
        #pragma omp parallel num_threads(opt_.num_threads)
        {
            set_affinity(omp_get_thread_num());
            // Initialize insert id for each thread
            auto id = opt_.num_records / opt_.num_threads * omp_get_thread_num();

            #pragma omp for schedule(static)
            for (uint64_t i = 0; i < opt_.num_records; ++i)
            {
                // Generate key in sequence
                auto key_ptr = key_generator_->hash_id(id++);

                static thread_local char value_out[value_generator_t::VALUE_MAX];
                bool found = tree_->find(key_ptr, key_generator_->size(), value_out);
                if (!found) {
                    std::cout << "Error: missing key!" << std::endl;
                    exit(1);
                }
            }
        }
    }

    std::cout << "Load verified; benchmark started." << std::endl;
}

void benchmark_t::run() noexcept
{
    std::vector<stats_t> global_stats;
    global_stats.resize(100000); // Avoid overhead of allocation and page fault
    global_stats.resize(0);

    static thread_local char value_out[value_generator_t::VALUE_MAX];
    char* values_out;

    std::vector<stats_t> local_stats(opt_.num_threads);
    for(auto& lc : local_stats)
    {
        if (opt_.bm_mode == mode_t::Operation)
        {
            lc.times.resize(std::ceil(opt_.num_ops/opt_.num_threads)*2);
        }
        else
        {
            lc.times.resize(1000000);
        }
        lc.times.resize(0);
    }

    // Control variable of monitor thread
    bool finished = false;

    // The amount of inserts expected to be done by each thread + some play room.
    uint64_t inserts_per_thread = 10 + (opt_.num_ops * opt_.insert_ratio) / opt_.num_threads;

    // Current id after load
    uint64_t current_id = key_generator_->current_id_;

    pid_t perf_pid;
    if (opt_.enable_perf)
    {
        std::cout << "Starting perf..." << std::endl;

        std::stringstream parent_pid;
        parent_pid << getpid();

        pid_t pid = fork();
        // Launch profiler
        if (pid == 0)
        {
            if (opt_.perf_record_args == "")
            {
                exit(execl("/usr/bin/perf", "perf", "record", "--call-graph", "dwarf", "-e", "cycles", "-p",
                        parent_pid.str().c_str(), nullptr));
            }
            else
            {
                std::vector<char *> argv;
                argv.push_back((char *)"perf");
                argv.push_back((char *)"record");

                std::string parent_pid_str = parent_pid.str();
                argv.push_back((char *)"-p");
                argv.push_back(const_cast<char *>(parent_pid_str.c_str()));

                // Ref: https://stackoverflow.com/a/5607650
                std::stringstream ss(opt_.perf_record_args);
                std::istream_iterator<std::string> begin(ss);
                std::istream_iterator<std::string> end;
                std::vector<std::string> args(begin, end);
                for (auto &s : args)
                {
                    argv.push_back(const_cast<char *>(s.c_str()));
                }

                // Ref: https://stackoverflow.com/a/35247904
                argv.push_back(nullptr);
                exit(execv("/usr/bin/perf", argv.data()));
            }
        }
        else
        {
            perf_pid = pid;
        }
    }

    std::unique_ptr<SystemCounterState> before_sstate;
    if (opt_.enable_pcm)
    {
        before_sstate = std::make_unique<SystemCounterState>();
        *before_sstate = getSystemCounterState();
    }

    double elapsed = 0.0;
    stopwatch_t sw;
    omp_set_nested(true);
    #pragma omp parallel sections num_threads(2)
    {
        #pragma omp section // Monitor thread
        {
            std::chrono::milliseconds sampling_window(opt_.sampling_ms);
            auto sample_stats = [&]()
            {
                std::this_thread::sleep_for(sampling_window);
                stats_t s;
                s.operation_count = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                                    [](uint64_t sum, const stats_t& curr) {
                                                        return sum + curr.operation_count;
                                                    });
                global_stats.push_back(std::move(s));
            };

            if (opt_.bm_mode == mode_t::Operation)
            {
                while (!finished)
                {
                    sample_stats();
                }
            }
            else
            {
                uint32_t iterations = opt_.seconds * 1000 / opt_.sampling_ms;
                uint32_t slept = 0;
                do {
                    sample_stats();
                }
                while (++slept < iterations);
                finished = true;
            }
        }

        #pragma omp section // Worker threads
        {
            #pragma omp parallel num_threads(opt_.num_threads)
            {
                auto tid = omp_get_thread_num();
                set_affinity(tid);

                // Initialize random seed for each thread
                key_generator_->set_seed(opt_.rnd_seed * (tid + 1));

                // Initialize insert id for each thread
                key_generator_->current_id_ = current_id + (inserts_per_thread * tid);

                foedus::assorted::BernoulliRandom random_bool(opt_.latency_sampling,
                                                              opt_.rnd_seed * (tid + 1));

                #pragma omp barrier

                #pragma omp single nowait
                {
                    sw.start();
                }

                auto execute_op = [&]()
                {
                    // Generate random operation
                    auto op = op_generator_.next();

                    // Generate random scrambled key
                    const char *key_ptr = nullptr;
                    if (op == operation_t::INSERT)
                    {
                        key_ptr = key_generator_->next(true);
                    }
                    else
                    {
                        auto id = key_generator_->next_id();
#if 0
                        // XXX(shiges): Ignore this in pseudo-self-similar.
                        if (opt_.bm_mode == mode_t::Time)
                        {
                            // Scale back to insert amount
                            id %= (local_stats[tid].success_insert_count * opt_.num_threads + opt_.num_records);
                            if (id >= opt_.num_records) {
                                uint64_t ins = id - opt_.num_records;
                                id = opt_.num_records + inserts_per_thread * (ins / local_stats[tid].success_insert_count) + ins % local_stats[tid].success_insert_count;
                            }
                        }
#endif
                        key_ptr = key_generator_->hash_id(id);
                    }

                    auto measure_latency = random_bool.next();
                    if (measure_latency)
                    {
                        local_stats[tid].times.push_back(std::chrono::high_resolution_clock::now());
                    }

                    run_op(op, key_ptr, value_out, values_out, measure_latency, local_stats[tid]);

                    if (measure_latency)
                    {
                        local_stats[tid].times.push_back(std::chrono::high_resolution_clock::now());
                    }
                };

#if defined(EPOCH_BASED_RECLAMATION)
                ART::ThreadInfo t(epoch_);
                uint32_t epoch_ops_threshold_count = 0;
                epoch_.enterEpoche(t);
#endif

                if (opt_.bm_mode == mode_t::Operation)
                {
                    #pragma omp for schedule(static)
                    for (uint64_t i = 0; i < opt_.num_ops; ++i)
                    {
                        execute_op();
#if defined(EPOCH_BASED_RECLAMATION)
                        if (++epoch_ops_threshold_count == opt_.epoch_ops_threshold) {
                            epoch_.exitEpocheAndCleanup(t);
                            epoch_.enterEpoche(t);
                            epoch_ops_threshold_count = 0;
                        }
#endif
                    }
                }
                else
                {
                    uint32_t slept = 0;
                    do
                    {
                        execute_op();
#if defined(EPOCH_BASED_RECLAMATION)
                        if (++epoch_ops_threshold_count == opt_.epoch_ops_threshold) {
                            epoch_.exitEpocheAndCleanup(t);
                            epoch_.enterEpoche(t);
                            epoch_ops_threshold_count = 0;
                        }
#endif
                    }
                    while (!finished);
                }

#if defined(EPOCH_BASED_RECLAMATION)
                epoch_.exitEpocheAndCleanup(t);
#endif

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

    if (opt_.enable_perf)
    {
        std::cout << "Stopping perf..." << std::endl;
        kill(perf_pid, SIGINT);
        waitpid(perf_pid, nullptr, 0);
    }

    std::unique_ptr<SystemCounterState> after_sstate;
    if (opt_.enable_pcm)
    {
        after_sstate = std::make_unique<SystemCounterState>();
        *after_sstate = getSystemCounterState();
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\tRun time: " << elapsed << " milliseconds" << std::endl;

    uint64_t total_ops = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                         [](uint64_t sum, const stats_t& curr) {
                                            return sum + curr.operation_count;
                                         });

    uint64_t total_success_ops = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                         [](uint64_t sum, const stats_t& curr) {
                                            return sum + curr.success_insert_count
                                                       + curr.success_read_count
                                                       + curr.success_update_count
                                                       + curr.success_remove_count
                                                       + curr.success_scan_count;
                                         });

    uint64_t total_insert = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                         [](uint64_t sum, const stats_t& curr) {
                                            return sum + curr.insert_count;
                                         });

    uint64_t total_success_insert = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                                    [](uint64_t sum, const stats_t& curr) {
                                                       return sum + curr.success_insert_count;
                                                    });

    uint64_t total_read = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                         [](uint64_t sum, const stats_t& curr) {
                                            return sum + curr.read_count;
                                         });

    uint64_t total_success_read = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                                    [](uint64_t sum, const stats_t& curr) {
                                                       return sum + curr.success_read_count;
                                                    });

    uint64_t total_update = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                         [](uint64_t sum, const stats_t& curr) {
                                            return sum + curr.update_count;
                                         });

    uint64_t total_success_update = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                                    [](uint64_t sum, const stats_t& curr) {
                                                       return sum + curr.success_update_count;
                                                    });

    uint64_t total_remove = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                         [](uint64_t sum, const stats_t& curr) {
                                            return sum + curr.remove_count;
                                         });

    uint64_t total_success_remove = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                                    [](uint64_t sum, const stats_t& curr) {
                                                       return sum + curr.success_remove_count;
                                                    });

    uint64_t total_scan = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                         [](uint64_t sum, const stats_t& curr) {
                                            return sum + curr.scan_count;
                                         });

    uint64_t total_success_scan = std::accumulate(local_stats.begin(), local_stats.end(), 0ull,
                                                    [](uint64_t sum, const stats_t& curr) {
                                                       return sum + curr.success_scan_count;
                                                    });


    if (opt_.bm_mode == mode_t::Operation && opt_.num_ops != total_ops)
    {
        std::cout << "Fatal: Total operations specified/performed don't match!";
        exit(1);
    }

    std::cout << "Results:\n";
    std::cout << "\tOperations: " << total_ops << std::endl;
    std::cout << "\tThroughput:\n" 
              << "\t- Completed: " << total_ops / ((double)elapsed / 1000) << " ops/s\n" 
              << "\t- Succeeded: " << total_success_ops / ((double)elapsed / 1000) << " ops/s\n" 
              << "\tBreakdown:\n"
              << "\t- Insert completed: " << total_insert / ((double)elapsed / 1000) << " ops/s\n"
              << "\t- Insert succeeded: " << total_success_insert / ((double)elapsed / 1000) << " ops/s\n"
              << "\t- Read completed: " << total_read / ((double)elapsed / 1000) << " ops/s\n"
              << "\t- Read succeeded: " << total_success_read / ((double)elapsed / 1000) << " ops/s\n"
              << "\t- Update completed: " << total_update / ((double)elapsed / 1000) << " ops/s\n"
              << "\t- Update succeeded: " << total_success_update / ((double)elapsed / 1000) << " ops/s\n"
              << "\t- Remove completed: " << total_remove / ((double)elapsed / 1000) << " ops/s\n"
              << "\t- Remove succeeded: " << total_success_remove/ ((double)elapsed / 1000) << " ops/s\n"
              << "\t- Scan completed: " << total_scan / ((double)elapsed / 1000) << " ops/s\n"
              << "\t- Scan succeeded: " << total_success_scan/ ((double)elapsed / 1000) << " ops/s"
              << std::endl;

    std::cout << "Per-thread breakdown (insert/read/update/remove/scan):\n";
    for (auto &s : local_stats) {
      std::cout << "\t" << s.insert_count << "/" 
                << s.read_count << "/" 
                << s.update_count << "/" 
                << s.remove_count << "/" 
                << s.scan_count << " completed" << std::endl;
    }

    for (auto &s : local_stats) {
      std::cout << "\t" << s.success_insert_count << "/" 
                << s.success_read_count << "/" 
                << s.success_update_count << "/" 
                << s.success_remove_count << "/" 
                << s.success_scan_count << " succeeded" << std::endl;
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

void benchmark_t::run_op(operation_t op, const char *key_ptr, 
                         char *value_out, char *values_out, bool measure_latency,
                         stats_t &stats)
{
    switch (op)
    {
    case operation_t::READ:
    {
        auto r = tree_->find(key_ptr, key_generator_->size(), value_out);
        ++stats.read_count;
        if (r)
        {
            ++stats.success_read_count;
        }
        break;
    }

    case operation_t::INSERT:
    {
        // Generate random value
        auto value_ptr = value_generator_.next();
        auto r = tree_->insert(key_ptr, key_generator_->size(), value_ptr, opt_.value_size);
        ++stats.insert_count;
        if (r)
        {
            ++stats.success_insert_count;
        }
        break;
    }

    case operation_t::UPDATE:
    {
        // Generate random value
        auto value_ptr = value_generator_.next();
        auto r = tree_->update(key_ptr, key_generator_->size(), value_ptr, opt_.value_size);
        ++stats.update_count;
        if (r)
        {
            ++stats.success_update_count;
        }
        break;
    }

    case operation_t::REMOVE:
    {
        auto r = tree_->remove(key_ptr, key_generator_->size());
        ++stats.remove_count;
        if (r)
        {
            ++stats.success_remove_count;
        }
        break;
    }

    case operation_t::SCAN:
    {
        auto r = tree_->scan(key_ptr, key_generator_->size(), opt_.scan_size, values_out);
        ++stats.scan_count;
        if (r)
        {
            ++stats.success_scan_count;
        }
        break;
    }

    default:
        std::cout << "Error: unknown operation!" << std::endl;
        exit(0);
        break;
    }
    ++stats.operation_count;
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
    case PiBench::distribution_t::RDTSC:
        return os << "RDTSC";
        break;
    case PiBench::distribution_t::PSEUDOSELFSIMILAR:
        return os << "PSEUDOSELFSIMILAR";
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
       << "\t# Threads: " << opt.num_threads << "\n"
       << (opt.bm_mode == PiBench::mode_t::Operation ? "\t# Operations: " : "\tDuration (s): ") << (opt.bm_mode == PiBench::mode_t::Operation ? opt.num_ops : opt.seconds) << "\n"
       << "\tSampling: " << opt.sampling_ms << " ms\n"
       << "\tLatency: " << opt.latency_sampling << "\n"
       << "\tKey prefix: " << opt.key_prefix << "\n"
       << "\tKey size: " << opt.key_size << "\n"
       << "\tValue size: " << opt.value_size << "\n"
       << "\tRandom seed: " << opt.rnd_seed << "\n"
       << "\tKey distribution: " << opt.key_distribution
       << (opt.key_distribution == PiBench::distribution_t::SELFSIMILAR || opt.key_distribution == PiBench::distribution_t::ZIPFIAN || opt.key_distribution == PiBench::distribution_t::PSEUDOSELFSIMILAR
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

#include "tree_api.hpp"
#include "benchmark.hpp"
#include "library_loader.hpp"
#include "cxxopts.hpp"

#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cerrno>

#include <dlfcn.h>

using namespace PiBench;

int main(int argc, char** argv)
{
    // Parse command line arguments
    options_t opt;
    tree_options_t tree_opt;
    try
    {
        cxxopts::Options options("PiBench", "Benchmark framework for persistent indexes.");
        options
            .positional_help("INPUT")
            .show_positional_help();

        options.add_options()
            ("input", "Absolute path to library file", cxxopts::value<std::string>())
            ("n,records", "Number of records to load", cxxopts::value<uint64_t>()->default_value("1000000"))
            ("p,operations", "Number of operations to execute", cxxopts::value<uint64_t>()->default_value("1000000"))
            ("t,threads", "Number of threads to use", cxxopts::value<uint32_t>()->default_value("1"))
            ("f,key_prefix", "Prefix string prepended to every key", cxxopts::value<std::string>()->default_value(""))
            ("k,key_size", "Size of keys in Bytes (without prefix)", cxxopts::value<uint32_t>()->default_value("4"))
            ("v,value_size", "Size of values in Bytes", cxxopts::value<uint32_t>()->default_value("4"))
            ("r,read_ratio", "Ratio of read operations", cxxopts::value<float>()->default_value("1"))
            ("i,insert_ratio", "Ratio of insert operations", cxxopts::value<float>()->default_value("0"))
            ("u,update_ratio", "Ratio of update operations", cxxopts::value<float>()->default_value("0"))
            ("d,remove_ratio", "Ratio of remove operations", cxxopts::value<float>()->default_value("0"))
            ("s,scan_ratio", "Ratio of scan operations", cxxopts::value<float>()->default_value("0"))
            ("scan_size", "Number of records to be scanned.", cxxopts::value<uint32_t>()->default_value("100"))
            ("sampling_ms", "Sampling window in milliseconds", cxxopts::value<uint32_t>()->default_value("1000"))
            ("distribution", "Key distribution to use", cxxopts::value<std::string>()->default_value("UNIFORM"))
            ("skew", "Key distribution skew factor to use", cxxopts::value<float>()->default_value("0.2"))
            ("seed", "Seed for random generators", cxxopts::value<uint32_t>()->default_value("1729"))
            ("pcm", "Turn on Intel PCM", cxxopts::value<bool>()->default_value("true"))
            ("pool_path", "Path to persistent pool", cxxopts::value<std::string>()->default_value(""))
            ("pool_size", "Size of persistent pool (in Bytes)", cxxopts::value<uint64_t>()->default_value("0"))
            ("skip_load", "Skip the load phase", cxxopts::value<bool>()->default_value("false"))
            ("latency_sampling", "Sample latency of requests", cxxopts::value<float>()->default_value("0"))
            ("help", "Print help")
        ;

        options.parse_positional({"input"});

        auto result = options.parse(argc, argv);
        if (result.count("help"))
        {
            std::cout << options.help() << std::endl;
            exit(0);
        }

        if (result.count("pcm"))
        {
            opt.enable_pcm = result["pcm"].as<bool>();
        }
        else
        {
            opt.enable_pcm = true;
        }

        if (result.count("skip_load"))
        {
            opt.skip_load = result["skip_load"].as<bool>();
        }
        else
        {
            opt.skip_load = false;
        }

        if (result.count("latency_sampling"))
        {
            opt.latency_sampling = result["latency_sampling"].as<float>();
        }
        else
        {
            opt.latency_sampling = 0.0;
        }

        if (result.count("input"))
        {
            opt.library_file = result["input"].as<std::string>();
        }
        else
        {
            std::cout << "Missing 'input' argument." << std::endl;
            std::cout << options.help() << std::endl;
            exit(0);
        }

        // Parse "num_records"
        if (result.count("records"))
            opt.num_records = result["records"].as<uint64_t>();
        else
            opt.num_records = 1e6;

        // Parse "num_operations"
        if (result.count("operations"))
            opt.num_ops = result["operations"].as<uint64_t>();
        else
            opt.num_ops = 1e6;

        // Parse "num_threads"
        if (result.count("threads"))
            opt.num_threads = result["threads"].as<uint32_t>();
        else
            opt.num_threads = 1;

        // Parse "sampling_ms"
        if (result.count("sampling_ms"))
            opt.sampling_ms = result["sampling_ms"].as<uint32_t>();
        else
            opt.sampling_ms = 1000;

        // Parse "key_prefix"
        if (result.count("key_prefix"))
            opt.key_prefix = result["key_prefix"].as<std::string>();
        else
            opt.key_prefix = "";

        // Parse "key_size"
        if (result.count("key_size"))
            opt.key_size = result["key_size"].as<uint32_t>();
        else
            opt.key_size = 4;

        // Parse "value_size"
        if (result.count("value_size"))
            opt.value_size = result["value_size"].as<uint32_t>();
        else
            opt.value_size = 4;

        // Parse "ops_ratio"
        if (result.count("read_ratio"))
            opt.read_ratio = result["read_ratio"].as<float>();
        else
            opt.read_ratio = 0;

        if (result.count("insert_ratio"))
            opt.insert_ratio = result["insert_ratio"].as<float>();
        else
            opt.insert_ratio = 0;

        if (result.count("update_ratio"))
            opt.update_ratio = result["update_ratio"].as<float>();
        else
            opt.update_ratio = 0;

        if (result.count("remove_ratio"))
            opt.remove_ratio = result["remove_ratio"].as<float>();
        else
            opt.remove_ratio = 0;

        if (result.count("scan_ratio"))
            opt.scan_ratio = result["scan_ratio"].as<float>();
        else
            opt.scan_ratio = 0;

        // Parse 'scan_size'.
        if (result.count("scan_size"))
            opt.scan_size = result["scan_size"].as<uint32_t>();
        else
            opt.scan_size = 100;

        // Parse 'key_distribution'
        if(result.count("distribution"))
        {
            std::string dist = result["distribution"].as<std::string>();
            std::transform(dist.begin(), dist.end(), dist.begin(), ::tolower);
            if(dist.compare("uniform") == 0)
                opt.key_distribution = distribution_t::UNIFORM;
            else if(dist.compare("selfsimilar") == 0)
                opt.key_distribution = distribution_t::SELFSIMILAR;
            else if(dist.compare("zipfian") == 0)
            {
                std::cout
                    << "WARNING: initializing ZIPFIAN generator might take time."
                    << std::endl;
                opt.key_distribution = distribution_t::ZIPFIAN;
            }
            else
            {
                std::cout << "Invalid key distribution, must be one of "
                << "[UNIFORM | SELFSIMILAR | ZIPFIAN], but is " << dist << std::endl;
                exit(1);
            }
        }
        else
            opt.key_distribution = distribution_t::UNIFORM;

        // Parse 'key_skew'
        if (result.count("skew"))
            opt.key_skew = result["skew"].as<float>();
        else
            opt.key_skew = 0.2;

        // Parse 'rnd_seed'
        if (result.count("seed"))
        {
            opt.rnd_seed = result["seed"].as<uint32_t>();
        }
        else
            opt.rnd_seed = 1729;

        // Parse "pool_path"
        if (result.count("pool_path"))
            tree_opt.pool_path = result["pool_path"].as<std::string>();
        else
            tree_opt.pool_path = "";

        // Parse "pool_size"
        if (result.count("pool_size"))
            tree_opt.pool_size = result["pool_size"].as<uint64_t>();
        else
            tree_opt.pool_size = 0;

    }
    catch (const cxxopts::OptionException& e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl;
        exit(1);
    }

    // Sanitize options
    if(opt.key_prefix.size() + opt.key_size > key_generator_t::KEY_MAX)
    {
        std::cout << "Total key size cannot be greater than " << key_generator_t::KEY_MAX
            << ", but is " << opt.key_prefix.size() + opt.key_size << std::endl;
        exit(1);
    }

    if(opt.value_size > value_generator_t::VALUE_MAX)
    {
        std::cout << "Total value size cannot be greater than " << value_generator_t::VALUE_MAX
            << ", but is " << opt.value_size << std::endl;
        exit(1);
    }

    auto sum = opt.read_ratio+opt.insert_ratio+opt.update_ratio+opt.remove_ratio+opt.scan_ratio;
    if (sum != 1.0)
    {
        std::cout << "Sum of ratios should be 1.0 but is " << sum << std::endl;
        exit(1);
    }

    if(opt.scan_size < 1 || opt.scan_size > benchmark_t::MAX_SCAN)
    {
        std::cout << "Scan size must be in the range [1," << value_generator_t::VALUE_MAX
            << "], but is " << opt.scan_size << std::endl;
        exit(1);
    }

    if(opt.key_distribution == distribution_t::SELFSIMILAR && (opt.key_skew < 0.0 || opt.key_skew > 0.5))
    {
        std::cout << "Skew factor must be in the range [0 , 0.5]." << std::endl;
        exit(1);
    }

    if(opt.key_distribution == distribution_t::ZIPFIAN && (opt.key_skew < 0.0 || opt.key_skew > 1.0))
    {
        std::cout << "Skew factor must be in the range [0.0 , 1.0]." << std::endl;
        exit(1);
    }

    if((opt.latency_sampling < 0.0 || opt.latency_sampling > 1.0))
    {
        std::cout << "Latency sampling must be in the range [0.0 , 1.0]." << std::endl;
        exit(1);
    }

    // Print env and options
    print_environment();
    std::cout << opt << std::endl;

    tree_opt.key_size = opt.key_prefix.size() + opt.key_size;
    tree_opt.value_size = opt.value_size;
    tree_opt.num_threads = opt.num_threads;

    library_loader_t lib(opt.library_file);
    tree_api* tree = lib.create_tree(tree_opt);
    if(tree == nullptr)
    {
        std::cout << "Error instantiating tree." << std::endl;
        exit(1);
    }

    benchmark_t bench(tree, opt);
    bench.load();
    bench.run();

    delete tree;
    return 0;
}

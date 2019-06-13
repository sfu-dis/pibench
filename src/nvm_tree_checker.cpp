#include "tree_api.hpp"
#include "library_loader.hpp"
#include "cxxopts.hpp"

#include <map>
#include <cstdlib>

int main(int argc, char** argv)
{
    std::string library_file;
    tree_options_t tree_opt;
    try
    {
        cxxopts::Options options("nvm_tree_checker", "Check utility for persistent trees.");
        options
            .positional_help("INPUT")
            .show_positional_help();

        options.add_options()
            ("input", "Absolute path to library file", cxxopts::value<std::string>())
            ("pool_path", "Path to persistent pool", cxxopts::value<std::string>()->default_value(""))
            ("pool_size", "Size of persistent pool (in Bytes)", cxxopts::value<uint64_t>()->default_value("0"))
        ;

        options.parse_positional({"input"});
        auto result = options.parse(argc, argv);
        if (result.count("help"))
        {
            std::cout << options.help() << std::endl;
            exit(0);
        }

        if (result.count("input"))
        {
            library_file = result["input"].as<std::string>();
        }
        else
        {
            std::cout << "Missing 'input' argument." << std::endl;
            std::cout << options.help() << std::endl;
            exit(0);
        }

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

    tree_opt.key_size = 8;
    tree_opt.value_size = 8;
    tree_opt.num_threads = 1;

    library_loader lib(library_file);
    tree_api* tree = lib.create_tree(tree_opt);
    if(tree == nullptr)
    {
        std::cout << "Error instantiating tree." << std::endl;
        exit(0);
    }

    // Mirror data structure used to crosscheck results.
    std::map<uint64_t, uint64_t> mirror;

    // Initialize random
    std::srand(1729);

    // Insert 10 million records
    const uint32_t nrecords = 10000000ULL;
    for(uint32_t i=0; i<nrecords; ++i)
    {
        uint64_t k = i;
        uint64_t v = std::rand();

        auto r1 = mirror.insert(std::make_pair(k,v));

        auto r2 = tree->insert(reinterpret_cast<const char*>(&k), sizeof(uint64_t), 
            reinterpret_cast<const char*>(&v), sizeof(uint64_t));

        if(r1.second != r2)
        {
            std::cout << "Different results for insert." << std::endl;
            exit(1);
        }
    }

    // Do 5 million lookups
    for(uint32_t i=0; i<5000000; ++i)
    {
        uint64_t k = std::rand() % nrecords;

        auto r1 = mirror.find(k);

        uint64_t value_out;
        auto r2 = tree->find(reinterpret_cast<const char*>(&k), sizeof(uint64_t),
            reinterpret_cast<char*>(&value_out));

        if (((r1 != mirror.end()) != r2)
            ||
            ((r2) && (r1->second != value_out)))
        {
            std::cout << "Different results for find." << std::endl;
            exit(1);
        }
    }

    // Do 5 million updates
    for(uint32_t i=0; i<5000000; ++i)
    {
        uint64_t k = std::rand() % nrecords;
        uint64_t v = std::rand();

        auto it = mirror.find(k);
        if(it != mirror.end())
            it->second = v;

        auto r = tree->update(reinterpret_cast<const char*>(&k), sizeof(uint64_t), 
            reinterpret_cast<const char*>(&v), sizeof(uint64_t));

        if ((it != mirror.end()) != r)
        {
            std::cout << "Different results for update." << std::endl;
            exit(1);
        }
    }

    // Do 5 million lookups
    for(uint32_t i=0; i<5000000; ++i)
    {
        uint64_t k = std::rand() % nrecords;

        auto r1 = mirror.find(k);

        uint64_t value_out;
        auto r2 = tree->find(reinterpret_cast<const char*>(&k), sizeof(uint64_t),
            reinterpret_cast<char*>(&value_out));

        if (((r1 != mirror.end()) != r2)
            ||
            ((r2) && (r1->second != value_out)))
        {
            std::cout << "Different results for find." << std::endl;
            exit(1);
        }
    }

    // Do 5 million deletes
    for(uint32_t i=0; i<5000000; ++i)
    {
        uint64_t k = std::rand() % nrecords;

        auto r1 = mirror.erase(k);
        auto r2 = tree->remove(reinterpret_cast<const char*>(&k), sizeof(uint64_t));

        if ((r1 > 0) != r2)
        {
            std::cout << "Different results for delete." << std::endl;
            exit(1);
        }
    }

    // Do 5 million lookups
    for(uint32_t i=0; i<5000000; ++i)
    {
        uint64_t k = std::rand() % nrecords;

        auto r1 = mirror.find(k);

        uint64_t value_out;
        auto r2 = tree->find(reinterpret_cast<const char*>(&k), sizeof(uint64_t),
            reinterpret_cast<char*>(&value_out));

        if (((r1 != mirror.end()) != r2)
            ||
            ((r2) && (r1->second != value_out)))
        {
            std::cout << "Different results for find." << std::endl;
            exit(1);
        }
    }

    // Do 1 million scans of 100 records
    for(uint32_t i=0; i<1000000; ++i)
    {
        uint64_t k = std::rand() % nrecords;

        auto it = mirror.lower_bound(k);
        char values_out1[4096];
        memset(values_out1, 0, 4096);
        char* dst = values_out1;
        int scanned = 0;
        for(scanned=0; (scanned < 100) && (it != mirror.end()); ++scanned,++it)
        {
            memcpy(dst, &it->first, sizeof(uint64_t));
            dst += sizeof(uint64_t);
            memcpy(dst, &it->second, sizeof(uint64_t));
            dst += sizeof(uint64_t);
        }

        char* values_out2;
        auto r2 = tree->scan(reinterpret_cast<const char*>(&k), sizeof(uint64_t),
            100, values_out2);

        if(scanned != r2 || memcmp(values_out1, values_out2, r2*(sizeof(uint64_t)+sizeof(uint64_t))) != 0)
        {
            std::cout << "Different result for scan." << std::endl;
            exit(1);
        }
    }

    // TODO: Close the tree and try to recover

    std::cout << "Success!" << std::endl;

    return 0;
}


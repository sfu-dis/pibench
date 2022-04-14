#ifndef __KEY_LOADER_HPP__
#define __KEY_LOADER_HPP__


#include <iostream>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <stdint.h>
#include <unistd.h>
#include <fstream>
#include <stdio.h>
#include <string.h>


class key_loader_t
{
public:
    key_loader_t();

    void fill_buffer();

    std::pair<char*, uint64_t> next();

    char const *filename = "../../datasets/examiner-date-text.txt";

    static thread_local uint64_t current_id_;
    static uint32_t get_seed() noexcept { return seed_; }
    static void set_seed(uint32_t seed)
    {
        seed_ = seed;
        generator_.seed(seed_);
    }

    uint64_t next_id()
    {
        return dist_(generator_);
    }

private:
    static thread_local uint32_t seed_;
    static thread_local std::default_random_engine generator_;
    std::uniform_int_distribution<uint64_t> dist_;

    uint64_t get_number_lines(char const *filename);

    uint64_t buffer_len;
    uint64_t* key_len;
    char** keys;
};


#endif
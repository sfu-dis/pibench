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


class key_loader_t
{
public:
    key_loader_t();

    void fill_buffer();

private:
    u_int64_t get_number_lines(char* filename);

    char* filename = "../datasets/new-born-names.txt";

    u_int64_t buffer_len;
    u_int64_t* key_len;
    char** keys;
};


#endif
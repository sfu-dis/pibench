#ifndef __KEY_LOADER_HPP__
#define __KEY_LOADER_HPP__


#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <stdint.h>
#include <unistd.h>


class key_loader_t
{
public:
    key_loader_t();

private:
    u_int64_t get_number_lines(char* filename);

    char* filename = "new-born-names.txt";

    char* keys[];
    char* values[];
    u_int64_t buffer_len;
};
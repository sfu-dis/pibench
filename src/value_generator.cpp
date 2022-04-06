#include "value_generator.hpp"

#include <cassert>
#include <cstring>

#include "utils.hpp"

namespace PiBench
{
thread_local foedus::assorted::UniformRandom value_generator_t::rng_;
thread_local char value_generator_t::buf_[KEY_MAX];

const char* value_generator_t::from_key(uint64_t key)
{
    memcpy(buf_, &key, sizeof(uint64_t));
    return buf_;
}
}
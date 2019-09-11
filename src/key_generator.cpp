#include "key_generator.hpp"
#include "utils.hpp"

namespace PiBench
{

thread_local std::default_random_engine key_generator_t::generator_;
thread_local uint32_t key_generator_t::seed_;
thread_local char key_generator_t::buf_[KEY_MAX];
thread_local uint64_t key_generator_t::current_id_ = 1;

key_generator_t::key_generator_t(size_t N, size_t size, const std::string& prefix)
    : N_(N),
      size_(size),
      prefix_(prefix)
{
    memset(buf_, 0, KEY_MAX);
    memcpy(buf_, prefix_.c_str(), prefix_.size());
}

const char* key_generator_t::next(bool in_sequence)
{
    char* ptr = &buf_[prefix_.size()];

    uint64_t id = in_sequence ? current_id_++ : next_id();
    uint64_t hashed_id = utils::multiplicative_hash<uint64_t>(id);

    if (size_ < sizeof(hashed_id))
    {
        // We want key smaller than 8 Bytes, so discard higher bits.
        auto bits_to_shift = (sizeof(hashed_id) - size_) << 3;

        // Discard high order bits
        if (utils::is_big_endian())
        {
            hashed_id >>= bits_to_shift;
            hashed_id <<= bits_to_shift;
        }
        else
        {
            hashed_id <<= bits_to_shift;
            hashed_id >>= bits_to_shift;
        }

        memcpy(ptr, &hashed_id, size_); // TODO: check if must change to fit endianess
    }
    else
    {
        // TODO: change this, otherwise zeroes act as prefix
        // We want key of at least 8 Bytes, check if we must prepend zeroes
        auto bytes_to_prepend = size_ - sizeof(hashed_id);
        if (bytes_to_prepend > 0)
        {
            memset(ptr, 0, bytes_to_prepend);
            ptr += bytes_to_prepend;
        }
        memcpy(ptr, &hashed_id, sizeof(hashed_id));
    }
    return buf_;
}
} // namespace PiBench
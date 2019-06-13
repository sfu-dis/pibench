#include "operation_generator.hpp"

namespace PiBench
{
thread_local uint32_t operation_generator_t::seed_;
thread_local std::default_random_engine operation_generator_t::gen_;
}
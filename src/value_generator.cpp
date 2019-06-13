#include "value_generator.hpp"

namespace PiBench
{
thread_local uint32_t value_generator_t::seed_;
thread_local std::default_random_engine value_generator_t::gen_;
}
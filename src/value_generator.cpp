#include "value_generator.hpp"

namespace PiBench
{
thread_local foedus::assorted::UniformRandom value_generator_t::rng_;
}
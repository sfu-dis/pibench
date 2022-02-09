#ifndef __RDTSC_HPP__
#define __RDTSC_HPP__

#include <cstdint>

namespace PiBench
{
    static uint64_t rdtsc(void)
    {
#if defined(__x86_64__)
        uint32_t low, high;
        asm volatile("rdtsc" : "=a"(low), "=d"(high));
        return (static_cast<uint64_t>(high) << 32) | static_cast<uint64_t>(low);
#else
#pragma message("Warning: unknown architecture, no rdtsc() support")
        return 0;
#endif
    }

}  // namespace PiBench

#endif

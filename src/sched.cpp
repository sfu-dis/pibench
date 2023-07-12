// The following implementation is adapted from CoroBase

#include <cstdint>
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <iostream>

#include <numa.h>
#include <sys/stat.h>

#include "sched.hpp"

std::vector<CPUCore> cpu_cores;

bool DetectCPUCores() {
  // FIXME(tzwang): Linux-specific way of querying NUMA topology
  //
  // We used to query /sys/devices/system/node/nodeX/cpulist to get a list of
  // all cores for this node, but it could be a comma-separated list (x, y, z)
  // or a range (x-y). So we just iterate each cpu dir here until dir not
  // found.
  struct stat info;
  if (stat("/sys/devices/system/node", &info) != 0) {
    return false;
  }

  for (uint32_t node = 0; node < numa_max_node() + 1; ++node) {
    uint32_t cpu = 0;
    while (cpu < std::thread::hardware_concurrency()) {
      std::string dir_name = "/sys/devices/system/node/node" +
                              std::to_string(node) + "/cpu" + std::to_string(cpu);
      struct stat info;
      if (stat(dir_name.c_str(), &info) != 0) {
        // Doesn't exist, continue to next to get all cores in the same node
        ++cpu;
        continue;
      }

      // Make sure it's a physical thread, not a hyper-thread: Query
      // /sys/devices/system/cpu/cpuX/topology/thread_siblings_list, if the
      // first number matches X, then it's a physical core [1] (might not work
      // in virtualized environments like Xen).  [1]
      // https://stackoverflow.com/questions/7274585/linux-find-out-hyper-threaded-core-id
      std::string sibling_file_name = "/sys/devices/system/cpu/cpu" +
                                      std::to_string(cpu) +
                                      "/topology/thread_siblings_list";
      char cpu_buf[8];
      memset(cpu_buf, 0, 8);
      std::vector<uint32_t> threads;
      std::ifstream sibling_file(sibling_file_name);
      while (sibling_file.good()) {
        memset(cpu_buf, 0, 8);
        sibling_file.getline(cpu_buf, 256, ',');
        threads.push_back(atoi(cpu_buf));
      }

      // A physical core?
      if (cpu == threads[0]) {
        cpu_cores.emplace_back(node, threads[0]);
        for (uint32_t i = 1; i < threads.size(); ++i) {
          cpu_cores[cpu_cores.size()-1].AddLogical(threads[i]);
        }
      }
      ++cpu;
    }
  }
  return true;
}

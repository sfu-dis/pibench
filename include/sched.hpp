#pragma once

#include <iostream>
#include <vector>

struct CPUCore {
  uint32_t node;
  uint32_t physical_thread;
  std::vector<uint32_t> logical_threads;
  CPUCore(uint32_t n, uint32_t phys) : node(n), physical_thread(phys) {}
  void AddLogical(uint32_t t) { logical_threads.push_back(t); }
  uint32_t operator[](size_t idx) {
    idx = idx % (logical_threads.size() + 1);
    return (idx == 0) ? physical_thread : logical_threads[idx - 1];
  }
};

extern std::vector<CPUCore> cpu_cores;
bool DetectCPUCores();

static void set_affinity(uint32_t thread_id) {
  int my_cpu_id = cpu_cores[thread_id % cpu_cores.size()][thread_id / cpu_cores.size()];
  cpu_set_t my_set;
  CPU_ZERO(&my_set);
  CPU_SET(my_cpu_id, &my_set);
  sched_setaffinity(0, sizeof(cpu_set_t), &my_set);
  //std::cout << "Pinned thread " << thread_id << " to CPU " << my_cpu_id << std::endl;
}

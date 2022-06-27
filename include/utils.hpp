#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace PiBench
{
namespace utils
{
    // Starting from 10
    static constexpr uint64_t POWER_TO_G[] = {29, 29, 53, 53, 117, 125, 229, 221,
    469, 517, 589, 861, 1189, 1653, 2333, 3381, 4629, 6565, 9293, 13093, 18509,
    26253, 37117, 52317, 74101, 104581, 147973, 209173, 296029, 418341, 91733,
    836661, 1183221, 1673485, 2366509, 3346853, 4732789, 6693237, 9465541,
    13386341, 18931141, 26772693, 37862197, 53545221, 75724373, 107090317};

    /**
     * @brief Calculates the discrete logarithmic of integer k.
     *
     * This function can be used to shuffle integers in a range without
     * collisions. The implementation is derived from:
     * "Quickly Generating Billion-Record Synthetic Databases", Jim Gray et al,
     * SIGMOD 19
     *
     * @tparam POWER_OF_TWO The nterval covered by the function is [1, 2^POWER_OF_TWO].
     * @param k
     * @return uint64_t
     */
    template<uint32_t POWER_OF_TWO>
    static uint64_t discrete_log(const uint64_t k) noexcept
    {
        static constexpr uint32_t POWER = POWER_OF_TWO + 2;
        static_assert(POWER >=10 && POWER <= 55, "POWER must be in range [10,55].");
        static constexpr uint64_t G = POWER_TO_G[POWER-10];
        static constexpr uint64_t P = 1ULL << (POWER-2);
        static constexpr uint64_t P_MASK = P-1;

        //assert(k > 0);
        assert(k < (1ULL << (POWER-2)));

        uint64_t up = k;
        uint64_t x = 0;
        uint64_t radix = 1;
        uint64_t Gpow = (G-1)/4;
        for(uint64_t i=0; up; ++i)
        {
            if(up & radix)
            {
                x = x + radix;
                up = (up + Gpow + 4 * up * Gpow) & P_MASK;
            }
            radix = radix << 1;
            Gpow = (Gpow + Gpow + 4 * Gpow * Gpow) & P_MASK;
        }
        return P-x;
    }

    template<typename T>
    static T fnv1a(const void* data, size_t size)
    {
        static_assert(
            std::is_same<T, uint32_t>::value || std::is_same<T, uint64_t>::value,
            "fnv1a only supports 32 bits and 64 bits variants."
        );

        static constexpr T INIT = std::is_same<T, uint32_t>::value ? 2166136261u : 14695981039346656037ul;
        static constexpr T PRIME = std::is_same<T, uint32_t>::value ? 16777619 : 1099511628211ul;

        auto src = reinterpret_cast<const char*>(data);
        T sum = INIT;
        while (size--)
        {
            sum = (sum ^ *src) * PRIME;
            src++;
        }
        return sum;
    }

    /**
     * @brief Calculate multiplicative hash of integer in the same domain.
     *
     * This was adapted from:
     *
     * ""The Art of Computer Programming, Volume 3, Sorting and Searching",
     * D.E. Knuth, 6.4 p:516"
     *
     * The function should be used as a cheap way to scramble integers in the
     * domain [0,2^T] to integers in the same domain [0,2^T]. If the resulting
     * hash should be in a smaller domain [0,2^m], the result should be right
     * shifted by X bits where X = (32 | 64) - m.
     *
     * @tparam T type of input and output (uint32_t or uint64_t).
     * @param x integer to be hashed.
     * @return T hashed result.
     */
    template<typename T>
    static T multiplicative_hash(T x)
    {
        static_assert(
            std::is_same<T, uint32_t>::value || std::is_same<T, uint64_t>::value,
            "multiplicative hash only supports 32 bits and 64 bits variants."
        );

        static constexpr T A = std::is_same<T, uint32_t>::value ?
            2654435761u
            :
            11400714819323198393ul;
        return A * x;
    }

    /**
     * @brief Verify endianess during runtime.
     *
     * @return true
     * @return false
     */
    static bool is_big_endian(void)
    {
        volatile union {
            uint32_t i;
            char c[4];
        } bint = {0x01020304};

        return bint.c[0] == 1;
    }

    /**
     * @brief Read memory without optimizing out.
     *
     * This function should be called to simulate a dummy use of the variable p.
     * It tells the compiler that there might be side effects and avoids any
     * optimization-out.
     *
     * Source: https://youtu.be/nXaxk27zwlk?t=2550
     *
     * @param addr beginning of memory region.
     * @param size size of memory region.
     */
    static void dummy_use(void* addr, size_t size)
    {
        char* p = static_cast<char*>(p);
        char* end = static_cast<char*>(p) + size;

        if(p >= end)
            return;

        for(; p<end; ++p)
        {
            void* vptr = static_cast<void*>(p);
            asm volatile("" : : "g"(vptr) : "memory");
        }
    }

    /**
     * @brief implements a thread barrier
     */
    class barrier {
    public:
      /**
       * @brief Construct a new barrier object
       *
       * @param Threshold threshold of the barrier
       */
      explicit barrier(std::uint64_t Threshold)
          : threshold(Threshold), capacity(Threshold) {}

      /**
       * @brief 'holds' the threads untill specified number of threads arrive at
       * the barrier. releases waiting threads at the same time.
       */
      void arriveAndWait() {
        std::unique_lock<std::mutex> lock(mtx);
        uint64_t localGeneration = generation;
        capacity--;

        if (capacity == 0) {
          generation++;
          capacity = threshold;
          cv.notify_all();
        } else {
          cv.wait(lock, [this, localGeneration] {
            return localGeneration != generation;
          });
        }
      }

    private:
      std::uint64_t threshold;
      std::uint64_t capacity;
      // used for preventing spurious wakeups
      std::uint64_t generation = 0;
      std::mutex mtx;
      std::condition_variable cv;
    };

    /**
     * @brief performs mathematical divison operation
     *
     * @param dividend dividend
     * @param divisor divisor
     * @return std::pair<uint64_t, uint64_t> pair containing quotient &
     * remainder respectively
     */
    inline std::pair<uint64_t, uint64_t> divide(const uint64_t dividend,
                                                const uint64_t divisor) {
      return {dividend / divisor, dividend % divisor};
    };

    /**
     * @brief get id of current thread
     *
     * @return uint32_t thread id
     */
    inline uint32_t getThreadId() {
      return std::hash<std::thread::id>{}(std::this_thread::get_id());
    }

    /**
     * @brief set affinity of a thread
     *
     * @param cores list of CPU cores that should be considered for pinning the
     * thread
     * @param threadId id of the thread to pin
     * @return true if affinity set successfully
     * @return false if failed to set affinity
     */
    inline bool setAffinity(const std::vector<uint32_t> &cores,
                            uint32_t threadId = getThreadId()) {
      if (cores.empty()) {
        return false;
      };

      int myCpuId = cores[threadId % cores.size()];
      cpu_set_t mySet;
      CPU_ZERO(&mySet);
      CPU_SET(myCpuId, &mySet);
      sched_setaffinity(0, sizeof(cpu_set_t), &mySet);
      return true;
    }

    /**
     * @brief runs a for loop parallelly. the work load is equally divided among
     * spcified number of threads.
     *
     * @param threadNum number of threads that should spawned for the workload
     * @param preLoopTask this function is called right before the loop is
     * executed
     * @param task this function is called in the for loop
     * @param iterations number of iterations for loop should perform
     */
    inline void parallelForLoop(
        const uint64_t threadNum, const std::vector<uint32_t> &cores,
        const std::function<void(uint64_t)> &preLoopTask,
        const std::function<void(uint64_t)> &task, const uint64_t iterations) {
      std::vector<std::thread> threads;
      barrier barr(threadNum);
      const auto partitionedIterations = divide(iterations, threadNum);

      for (uint64_t j = 0; j < threadNum; j++) {
        threads.emplace_back(std::thread([&, partitionedIterations, j]() {
          setAffinity(cores);
          uint64_t localThreadId = j;
          barr.arriveAndWait();

          uint64_t threadLoad = j == 0 ? partitionedIterations.first +
                                             partitionedIterations.second
                                       : partitionedIterations.first;
          try {
            preLoopTask(localThreadId);
          } catch (std::exception &ex) {
            std::cerr << "exception thrown in the pre-loop task: " << ex.what()
                      << '\n';
          };
          for (uint64_t i = 0; i < threadLoad; i++) {
            try {
              task(localThreadId);
            } catch (std::exception &ex) {
              std::cerr << "exception thrown in the task: " << ex.what()
                        << '\n';
            };
          };
        }));
      };

      for (auto &i : threads) {
        i.join();
      };
    };
} // namespace utils
} // namespace PiBench
#endif
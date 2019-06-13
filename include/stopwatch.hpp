#ifndef __STOPWATCH_HPP__
#define __STOPWATCH_HPP__

#include <chrono>
#include <ratio>
#include <type_traits>

#include <iostream>

namespace PiBench
{

template <typename T>
struct is_duration : std::false_type
{
};

template <class Rep, std::intmax_t Num, std::intmax_t Denom>
struct is_duration<std::chrono::duration<Rep, std::ratio<Num, Denom>>> : std::true_type
{
};

/**
 * @brief Wrapper that implements stopwatch functionality.
 *
 */
class stopwatch_t
{
public:
    stopwatch_t() noexcept {}
    ~stopwatch_t() noexcept {}

    /**
     * @brief Start time counter.
     *
     */
    void start() noexcept
    {
        start_ = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Clear time counter.
     *
     */
    void clear() noexcept
    {
        start_ = {};
    }

    /**
     * @brief Returns amount of elapsed time.
     *
     * @tparam T unit used to return.
     * @return float
     */
    template <typename T>
    float elapsed() noexcept
    {
        static_assert(is_duration<T>::value);

        auto stop = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, typename T::period> e = stop - start_;
        return e.count();
    }

    /**
     * @brief Returns true if given duration is elapsed since last call.
     *
     * @tparam T unit used by duration.
     * @param d duration to be checked.
     * @return true
     * @return false
     */
    template <typename T>
    bool is_elapsed(const T& d) noexcept
    {
        static_assert(is_duration<T>::value);

        static auto prev = start_;

        auto stop = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, typename T::period> e = stop - prev;

        if (e >= d)
        {
            prev = stop;
            return true;
        }
        else
            return false;
    }

private:
    /// Time point when the stopwatched started.
    std::chrono::high_resolution_clock::time_point start_;
};
} // namespace PiBench
#endif
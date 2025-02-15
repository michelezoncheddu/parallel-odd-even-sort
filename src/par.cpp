/**
 * @file   par.cpp
 * @brief  STD thread parallel implementation of the odd-even sort algorithm
 * @author Michele Zoncheddu
 */


#include <algorithm>  // std::is_sorted
#include <cassert>
#include <cmath>      // for ceil
#include <functional> // std::ref, std::cref
#include <iostream>
#include <memory>     // Smart pointers
#include <thread>
#include <vector>

#include <barrier.hpp>
#include <config.hpp>
#include <util.hpp>

short cache_padding;
bool finished = false;

/**
 * @brief It performs an odd or an even sorting phase on the array.
 *
 * @tparam T the vector pointer type
 * @param v the pointer to the vector
 * @param phase the phase (odd or even)
 * @param end the end of the array
 * @return the number of swaps
 */
template <typename T>
unsigned odd_even_sort(T * const v, short const phase, size_t const end) {
    unsigned swaps = 0;
    for (size_t i = phase; i < end; i += 2) {
        auto first = v[i], second = v[i + 1];
        auto cond = first > second;
        v[i]     = cond ? second : first;
        v[i + 1] = cond ? first : second;
        if (v[i] != first)
            swaps++;
    }
    return swaps;
}

/**
 * @brief The business logic of the worker.
 *
 * @tparam T the vector pointer type
 * @param thid the thread identifier
 * @param v the pointer to the vector
 * @param end the end position (included)
 * @param offset if false, the odd positions in the pointer are odd positions in the whole array,
 *               if true, the odd positions in the pointer are even positions in the whole array.
 * @param phases the vector of phases progress
 * @param swaps the vector of swaps
 * @param barriers the synchronization barriers
 */
template <typename T>
void thread_body(int thid, T * const v, size_t const end, bool const offset, int const nw,
                 std::vector<unsigned> &phases,
                 std::vector<unsigned> &swaps,
                 std::vector<std::unique_ptr<barrier>> const &barriers) {
    auto iter = 0;
    auto const pos = thid * cache_padding; // Cache-aware position in phases and swaps array
    auto const has_left_neigh = thid > 0, has_right_neigh = thid < nw - 1;

    /*
     * I know that repeating code is bad practice, but in this case
     * is the only way to achieve better performances.
     * The key for the performance lies in the explicit '1' and '0' in the function call,
     * and in the asynchronous wait for the neighbours threads.
     */
    if (!offset) {
        while (!finished) {
            swaps[pos] |= odd_even_sort(v, 1, end); // Odd phase

            phases[pos]++; // Ready for the next phase

            // Wait my neighbours to be ready
            if (has_right_neigh)
                while (phases[pos] != phases[(thid + 1) * cache_padding])
                    __asm__("nop"); // To force the compiler to don't "optimize" this loop
            if (has_left_neigh)
                while (phases[pos] != phases[(thid - 1) * cache_padding])
                    __asm__("nop");

            swaps[pos] |= odd_even_sort(v, 0, end); // Even phase

            barriers[iter++]->wait();
            swaps[pos] = 0;
        }
    } else {
        while (!finished) {
            swaps[pos] |= odd_even_sort(v, 0, end); // Odd phase

            phases[pos]++; // Ready for the next phase

            // Wait my neighbours to be ready
            if (has_right_neigh)
                while (phases[pos] != phases[(thid + 1) * cache_padding])
                    __asm__("nop"); // To force the compiler to don't "optimize" this loop
            if (has_left_neigh)
                while (phases[pos] != phases[(thid - 1) * cache_padding])
                    __asm__("nop");

            swaps[pos] |= odd_even_sort(v, 1, end); // Even phase

            barriers[iter++]->wait();
            swaps[pos] = 0;
        }
    }
}

/**
 * @brief The business logic of the controller: checks in real time if there are swaps,
 *        to keep the workers running or to stop them.
 *
 * @param swaps the vector of swaps
 * @param barriers the synchronization barriers
 */
void controller_body(std::vector<unsigned> const &swaps, std::vector<std::unique_ptr<barrier>> const &barriers) {
    auto iter = 0;

    while (true) {
        unsigned local_swaps = 0;

        // While there are no swaps and some worker is still running...
        while (!local_swaps && barriers[iter]->read() > 1) {
            for (size_t i = 0; i < swaps.size(); i += cache_padding)
                local_swaps |= swaps[i];
        }

        // If there are no swaps, search again (I might have missed the last one)
        size_t i = 0;
        while (!local_swaps && i < swaps.size()) {
            local_swaps |= swaps[i];
            i += cache_padding;
        }

        // No swaps, end of the computation
        if (!local_swaps) {
            finished = true;
            barriers[iter]->dec();
            return;
        }

        barriers[iter++]->wait();
    }
}

/**
 * @brief the starting method
 *
 * @return the exit status
 */
int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage is " << argv[0]
                  << " n nw [seed] [cache-line size]" << std::endl;
        return -1;
    }

    auto const n  = strtol(argv[1], nullptr, 10); // Array length
    auto const nw = static_cast<int>(strtol(argv[2], nullptr, 10));

    if (n < 1 || nw < 1) {
        std::cout << "n and nw must be greater than zero" << std::endl;
        return -1;
    }

    if (n < nw) {
        std::cout << "nw must be greater than n" << std::endl;
        return -1;
    }

    // Create the vector
    std::vector<vec_type> v;
    if (argc > 3)
        v = create_random_vector<vec_type>(n, MIN, MAX, strtol(argv[3], nullptr, 10));
    else
        v = create_random_vector<vec_type>(n, MIN, MAX);
    auto const ptr = v.data();

    // Setting the cache_padding
    if (argc > 4)
        cache_padding = ceil(static_cast<double>(strtol(argv[4], nullptr, 10)) / sizeof(unsigned));
    else
        cache_padding = 64 / sizeof(unsigned);

    auto const start_time = std::chrono::system_clock::now();

    std::vector<std::unique_ptr<barrier>> barriers(n); // n is an upper bound for the number of iterations
    for (auto &elem : barriers)
        elem = std::make_unique<barrier>(nw + 1); // + 1 for the controller

    std::vector<unsigned> phases(nw * cache_padding, 0);
    std::vector<unsigned> swaps(nw * cache_padding, 0);

    std::vector<std::unique_ptr<std::thread>> workers;
    workers.reserve(nw);

    size_t const chunk_len = (v.size() - 1) / nw;
    long remaining = static_cast<long>((v.size() - 1) % nw);
    size_t offset = 0;

    std::thread controller(controller_body, std::cref(swaps), std::cref(barriers));

    for (int i = 0; i < nw; ++i) {
        workers.push_back(std::make_unique<std::thread>(
                thread_body<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), offset % 2, nw,
                std::ref(phases), std::ref(swaps), std::cref(barriers)));
        offset += chunk_len + (remaining > 0);
        --remaining;
    }

#ifdef LINUX_MACHINE
    // Thread pinning
    auto const hw_concurrency = std::thread::hardware_concurrency();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (0 != pthread_setaffinity_np(controller.native_handle(), sizeof(cpu_set_t), &cpuset)) {
        std::cout << "Error in thread pinning" << std::endl;
        return EXIT_FAILURE;
    }
    for (int i = 0; i < nw; ++i) {
        CPU_ZERO(&cpuset);
        CPU_SET((i + 1) % hw_concurrency, &cpuset);
        if (0 != pthread_setaffinity_np(workers[i]->native_handle(), sizeof(cpu_set_t), &cpuset)) {
            std::cout << "Error in thread pinning" << std::endl;
            return EXIT_FAILURE;
        }
    }
#endif

    controller.join();
    for (auto &thread : workers)
        thread->join();
    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << "Time: " << duration << " ms" << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));

    return 0;
}

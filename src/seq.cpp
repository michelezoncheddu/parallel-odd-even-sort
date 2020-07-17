/**
 * @file   seq.cpp
 * @brief  Sequential implementation of the odd-even sort algorithm
 * @author Michele Zoncheddu
 */


#include <algorithm> // std::is_sorted
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

#include <config.hpp>
#include <util.hpp>

/**
 * @brief It performs an odd or an even sorting phase on the array
 *
 * @tparam T the vector type
 * @param v the vector
 * @param start the phase (0: even, 1: odd)
 * @return the number of swaps
 */
template <typename T>
unsigned odd_even_sort(std::vector<T> &v, short start) {
    unsigned swaps = 0;
    for (size_t i = start; i < v.size() - 1; i += 2) {
        auto first = v[i], second = v[i + 1];
        auto cond = first > second;
        v[i] = cond ? second : first;
        v[i + 1] = cond ? first : second;
        if (v[i] != first)
            swaps++;
    }
    return swaps;
}

/**
 * @brief the starting method
 *
 * @return the exit status
 */
int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cout << "Usage is " << argv[0]
                  << " n" << std::endl;
        return -1;
    }

    auto const n = strtol(argv[1], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);
    unsigned swaps;

    // Dirty trick for getting the current thread handle
    auto thread_id = std::this_thread::get_id();
    auto native_handle = *reinterpret_cast<std::thread::native_handle_type*>(&thread_id);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (0 != pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &cpuset)) {
        std::cout << "Error in thread pinning" << std::endl;
        return EXIT_FAILURE;
    }

    auto const start_time = std::chrono::system_clock::now();
    do {
        swaps  = odd_even_sort(v, 1); // Odd phase
        swaps += odd_even_sort(v, 0); // Even phase
    } while (swaps > 0);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

/**
 * @file   par_local.cpp
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

std::vector<vec_type> shared;
bool finished = false;

unsigned cache_padding;
unsigned constexpr padding_2 = 64 / sizeof(vec_type) / 2;

/**
 * @brief It performs an odd or an even sorting phase on the array
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

template <typename T>
void thread_body(int thid, T * const v, size_t const end, bool const offset, int const nw,
                 std::vector<unsigned> &phases,
                 std::vector<unsigned> &swaps,
                 std::vector<std::unique_ptr<barrier>> const &barriers) {
    std::vector<vec_type> local_vector;
    for (size_t i = 0; i < end; ++i)
        local_vector.push_back(v[i]);

    auto pos = thid * cache_padding;

    auto iter = 0;
    auto const my_end = local_vector.size() - 1;
    auto const has_left_neigh = thid > 0, has_right_neigh = thid < nw - 1;

    while (!finished) {
        swaps[pos] |= odd_even_sort(local_vector.data(), !offset, my_end);
        if (has_right_neigh && my_end % 2 != offset) {
            if (local_vector[my_end] > shared[(thid + 1) * padding_2]) {
                std::swap(local_vector[my_end], shared[(thid + 1) * padding_2]);
                swaps[pos] |= true;
            }
        }

        if (!offset)
            local_vector[0] = shared[thid * padding_2];
        else
            shared[thid * padding_2] = local_vector[0];

        phases[pos]++; // Ready for the next phase

        // Wait my neighbours to be ready
        if (has_right_neigh)
            while (phases[pos] != phases[(thid + 1) * cache_padding])
                    __asm__("nop"); // To force the compiler to don't "optimize" this loop
        if (has_left_neigh)
            while (phases[pos] != phases[(thid - 1) * cache_padding])
                    __asm__("nop");

        swaps[pos] |= odd_even_sort(local_vector.data(), offset, my_end);
        if (has_right_neigh && my_end % 2 == offset) {
            if (local_vector[my_end] > shared[(thid + 1) * padding_2]) {
                std::swap(local_vector[my_end], shared[(thid + 1) * padding_2]);
                swaps[pos] |= true;
            }
        }

        if (offset)
            local_vector[0] = shared[thid * padding_2];
        else
            shared[thid * padding_2] = local_vector[0];

        barriers[iter++]->wait();
        swaps[pos] = 0;
    }

    for (auto i = 0; i < local_vector.size(); ++i)
        v[i] = local_vector[i];
}

void controller_body(std::vector<std::unique_ptr<barrier>> const &barriers, std::vector<unsigned> const &swaps) {
    int iter = 0;

    while (true) {
        unsigned local_swaps = 0;

        while (!local_swaps && barriers[iter]->read() > 1) {
            for (size_t i = 0; i < swaps.size(); i += cache_padding)
                local_swaps |= swaps[i];
        }

        size_t i = 0;
        while (!local_swaps && i < swaps.size()) {
            local_swaps |= swaps[i];
            i += cache_padding;
        }

        if (!local_swaps) {
            finished = true;
            barriers[iter]->dec();
            return;
        }

        barriers[iter++]->wait();
    }
}

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

    // HERE

    shared = std::vector<vec_type>(nw * padding_2);

    std::vector<std::unique_ptr<barrier>> barriers;
    barriers.resize(n + 1);
    for (auto &elem : barriers)
        elem = std::make_unique<barrier>(nw + 1);

    std::vector<std::unique_ptr<std::thread>> threads(nw);
    std::vector<unsigned> swaps(nw * cache_padding, 0);
    std::vector<unsigned> phases(nw * cache_padding, 0);

    size_t const chunk_len = (v.size() - 1) / nw;
    int remaining = static_cast<int>((v.size() - 1) % nw);
    size_t offset = 0;

    auto const start_time = std::chrono::system_clock::now();

    std::thread controller(controller_body, std::ref(barriers), std::ref(swaps));

    for (unsigned i = 0; i < nw; ++i) {
        shared[i * padding_2] = v[offset];
        offset += chunk_len + (remaining > 0);
        --remaining;
    }

    offset = 0;
    remaining = static_cast<int>((v.size() - 1) % nw);

    for (unsigned i = 0; i < nw - 1; ++i) {
        threads[i] = std::make_unique<std::thread>(
                thread_body<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), offset % 2, nw,
                std::ref(phases), std::ref(swaps), std::cref(barriers));
        offset += chunk_len + (remaining > 0);
        --remaining;
    }
    threads[threads.size() - 1] = std::make_unique<std::thread>(
            thread_body<vec_type>, threads.size() - 1, ptr + offset, chunk_len + 1, offset % 2, nw,
            std::ref(phases), std::ref(swaps), std::cref(barriers));

    // Thread pinning (works only on Linux)
    cpu_set_t cpuset;
    for (int i = 0; i < nw; ++i) {
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        if (0 != pthread_setaffinity_np(threads[i]->native_handle(), sizeof(cpu_set_t), &cpuset)) {
            std::cout << "Error in thread pinning" << std::endl;
            return EXIT_FAILURE;
        }
    }
    CPU_ZERO(&cpuset);
    CPU_SET(nw, &cpuset);
    if (0 != pthread_setaffinity_np(controller.native_handle(), sizeof(cpu_set_t), &cpuset)) {
        std::cout << "Error in thread pinning" << std::endl;
        return EXIT_FAILURE;
    }

    controller.join();
    for (auto &thread : threads)
        thread->join();
    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

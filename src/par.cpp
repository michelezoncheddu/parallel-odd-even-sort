#include <algorithm>
#include <atomic>
#include <chrono>
#include <cassert>
#include <iostream>
#include <thread>

#include <barrier.hpp>
#include <config.hpp>
#include <util.hpp>

unsigned nw;

// Padding for the phases and swap array
unsigned constexpr padding = 64 / sizeof(unsigned); // TODO: read from input (at least)

bool finished = false;

template <typename T>
unsigned odd_even_sort(T * const v, short phase, size_t end) {
    unsigned swaps = 0;
    for (size_t i = phase; i < end; i += 2) {
        auto first = v[i], second = v[i + 1];
        auto cond = first > second;
        v[i] = cond ? second : first;
        v[i + 1] = cond ? first : second;
        if (v[i] != first)
            swaps++;
    }
    return swaps;
}

template <typename T>
void thread_body(unsigned thid, T * const v, size_t end, short alignment,
        std::vector<unsigned> &phases,
        std::vector<unsigned> &swaps,
        std::vector<std::unique_ptr<barrier>> const &barriers) {
    auto iter = 0;
    auto const pos = thid * padding; // Cache-aware position in phases and swaps array
    auto const has_left_neigh = thid > 0, has_right_neigh = thid < nw - 1;
    
    while (!finished) {
        swaps[pos] |= odd_even_sort(v, !alignment, end); // Odd phase
        
        phases[pos]++; // Ready for the next phase

        // Wait my neighbours to be ready
        if (has_right_neigh)
            while (phases[pos] != phases[(thid + 1) * padding])
                __asm__("nop"); // To force the compiler to don't "optimize" this loop
        if (has_left_neigh)
            while (phases[pos] != phases[(thid - 1) * padding])
                __asm__("nop");

        swaps[pos] |= odd_even_sort(v, alignment, end); // Even phase
        barriers[iter++]->wait();
    }
}

void controller_body(std::vector<unsigned> &swaps, std::vector<std::unique_ptr<barrier>> const &barriers) {
    auto iter = 0;
    while (true) {
        unsigned local_swaps = 0;

        // While there are no swaps and some worker is still running...
        while (!local_swaps && barriers[iter]->read() > 1) {
            for (size_t i = 0; i < swaps.size(); i += padding)
                local_swaps |= swaps[i];
        }

        // If there are no swaps, search again (I might have missed the last one)
        size_t i = 0;
        while (!local_swaps && i < swaps.size()) {
            local_swaps |= swaps[i];
            i += padding;
        }

        // No swaps, end of the computation
        if (!local_swaps) {
            finished = true;
            barriers[iter]->dec();
            return;
        }

        // Reset the swap array and go on
        for (size_t i = 0; i < swaps.size(); i += padding)
            swaps[i] = 0;
        barriers[iter++]->wait();
    }
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage is " << argv[0]
                  << " n nw" << std::endl;
        return -1;
    }

    auto const n = strtol(argv[1], nullptr, 10); // Array length
    nw = strtol(argv[2], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);
    auto const ptr = v.data();

    auto const start_time = std::chrono::system_clock::now();

    std::vector<std::unique_ptr<barrier>> barriers(n); // n is an upper bound of the number of iterations
    for (auto &elem : barriers)
        elem = std::make_unique<barrier>(nw + 1); // + 1 for the controller

    std::vector<unsigned> phases(nw * padding, 0);
    std::vector<unsigned> swaps(nw * padding, 0);

    std::vector<std::unique_ptr<std::thread>> threads;
    threads.reserve(nw);

    size_t const chunk_len = v.size() / nw;
    int remaining = static_cast<int>(v.size() % nw);
    size_t offset = 0;

    std::thread controller(controller_body, std::ref(swaps), std::ref(barriers));

    for (unsigned i = 0; i < nw - 1; ++i) {
        threads.push_back(std::make_unique<std::thread>(
                thread_body<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), offset % 2,
                std::ref(phases), std::ref(swaps), std::ref(barriers)));
        offset += chunk_len + (remaining > 0);
        --remaining;
    }
    threads.push_back(std::make_unique<std::thread>(
            thread_body<vec_type>, nw - 1, ptr + offset, chunk_len - 1, offset % 2,
            std::ref(phases), std::ref(swaps), std::ref(barriers)));

    // Thread pinning (works only on Linux)
    cpu_set_t cpuset;
    for (unsigned i = 0; i < nw; ++i) {
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

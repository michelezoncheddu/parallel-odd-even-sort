#include <algorithm>
#include <atomic>
#include <chrono>
#include <cassert>
#include <iostream>
#include <thread>

#include <barrier.hpp>
#include <config.hpp>
#include <util.hpp>

// Padding for the swap array
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
void thread_body(unsigned thid, T * const v, size_t end, int alignment,
        std::vector<std::unique_ptr<barrier>> const &barriers1,
        std::vector<std::unique_ptr<barrier>> const &barriers2,
        std::vector<unsigned> &swaps) {
    auto iter = 0;
    auto const pos = thid * padding;
    while (!finished) {
        swaps[pos] |= odd_even_sort(v, !alignment, end); // Odd phase
        barriers1[iter]->wait();
        swaps[pos] |= odd_even_sort(v, alignment, end); // Even phase
        barriers2[iter++]->wait();
    }
}

void controller_body(std::vector<std::unique_ptr<barrier>> const &barriers, std::vector<unsigned> &swaps) {
    auto iter = 0;
    while (true) {
        unsigned tmp = 0;
        while (!tmp && barriers[iter]->read() > 1) {
            for (size_t i = 0; i < swaps.size(); i += padding)
                tmp |= swaps[i];
        }

        if (!tmp) { // TODO: needed? Test performance.
            size_t i = 0;
            while (!tmp && i < swaps.size()) {
                tmp |= swaps[i];
                i += padding;
            }
        }

        if (!tmp) {
            finished = true;
            barriers[iter]->dec();
            return;
        }

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

    auto const n  = strtol(argv[1], nullptr, 10);
    auto const nw = strtol(argv[2], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);
    auto const ptr = v.data();

    auto const start_time = std::chrono::system_clock::now();

    // TODO: rename variables
    std::vector<std::unique_ptr<barrier>> barriers1(n), barriers2(n); // n is an upper bound for the number of iterations
    for (auto &elem : barriers1)
        elem = std::make_unique<barrier>(nw);
    for (auto &elem : barriers2)
        elem = std::make_unique<barrier>(nw + 1); // plus the controller

    std::vector<std::unique_ptr<std::thread>> threads;
    threads.reserve(nw);

    std::vector<unsigned> swaps(nw * padding, 0);

    size_t const chunk_len = v.size() / nw;
    int remaining = static_cast<int>(v.size() % nw);
    size_t offset = 0;

    std::thread controller(controller_body, std::ref(barriers2), std::ref(swaps));

    for (auto i = 0; i < nw - 1; ++i) {
        threads.push_back(std::make_unique<std::thread>(
                thread_body<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), offset % 2,
                std::ref(barriers1), std::ref(barriers2), std::ref(swaps)));
        offset += chunk_len + (remaining > 0);
        --remaining;
    }
    threads.push_back(std::make_unique<std::thread>(
            thread_body<vec_type>, nw - 1, ptr + offset, chunk_len - 1, offset % 2,
            std::ref(barriers1), std::ref(barriers2), std::ref(swaps)));

    // Thread pinning (works only on Linux) (TODO: hw concurrency awareness)
    cpu_set_t cpuset;
    for (auto i = 0; i < nw; ++i) {
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

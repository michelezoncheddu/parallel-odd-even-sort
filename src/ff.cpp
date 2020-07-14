#include <algorithm>
#include <atomic>
#include <chrono>
#include <cassert>
#include <iostream>

#include <config.hpp>
#include <util.hpp>

#include <ff/ff.hpp>
#include <ff/farm.hpp>

using namespace ff;

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

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage is " << argv[0]
                  << " n nw" << std::endl;
        return -1;
    }

    auto const n  = strtol(argv[1], nullptr, 10); // Array length
    auto const nw = strtol(argv[2], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);
    auto const ptr = v.data();

    auto const start_time = std::chrono::system_clock::now();

//    size_t const chunk_len = v.size() / nw;
//    int remaining = static_cast<int>(v.size() % nw);
//    size_t offset = 0;
//
//    for (unsigned i = 0; i < nw - 1; ++i) {
//        threads.push_back(std::make_unique<std::thread>(
//                thread_body<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), offset % 2,
//                std::ref(phases), std::ref(swaps), std::ref(barriers)));
//        offset += chunk_len + (remaining > 0);
//        --remaining;
//    }
//    threads.push_back(std::make_unique<std::thread>(
//            thread_body<vec_type>, nw - 1, ptr + offset, chunk_len - 1, offset % 2,
//            std::ref(phases), std::ref(swaps), std::ref(barriers)));

    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));

    return 0;
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cassert>
#include <iostream>
#include <thread>

#include <barrier.hpp>
#include <config.hpp>
#include <util.hpp>

std::atomic<int> global_swaps = 0;

template <typename T>
int odd_even_sort(T *v, short phase, size_t end) {
    auto swaps = 0;
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
void f(unsigned thid, T *v, size_t end, int alignment, std::vector<std::unique_ptr<barrier>> &barriers) {
    auto iter = 0;
    while (true) {
        if (thid == 0)
            global_swaps = 0;
        bool odd = odd_even_sort(v, !alignment, end);
        barriers[iter++]->wait();
        bool even = odd_even_sort(v, alignment, end);
        global_swaps |= (odd || even);
        barriers[iter++]->wait();
        if (!odd && !even && !global_swaps)
            break;
        barriers[iter++]->wait();
    }

    /*do { // slower
        barriers[iter++]->wait();
        if (thid == 0)
            global_swaps = 0;
        odd = odd_even_sort(v, !alignment, end);
        barriers[iter++]->wait();
        even = odd_even_sort(v, alignment, end);
        global_swaps |= (odd || even);
        barriers[iter++]->wait();
    } while (odd || even || global_swaps);*/
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

    //barrier barrier(nw);
    std::vector<std::unique_ptr<barrier>> barriers;
    barriers.resize(n * 3);
    for (auto &elem : barriers)
        elem = std::make_unique<barrier>(nw);

    std::vector<std::unique_ptr<std::thread>> threads(nw);

    size_t const chunk_len = v.size() / nw;
    int remaining = static_cast<int>(v.size() % nw);
    size_t offset = 0;

    auto const start_time = std::chrono::system_clock::now();
    for (unsigned i = 0; i < nw - 1; ++i) {
        //std::cout << offset << " 1, " << chunk_len + (remaining > 0) << std::endl;
        threads[i] = std::make_unique<std::thread>(
                f<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), offset % 2, std::ref(barriers));
        offset += chunk_len + (remaining > 0);
        --remaining;
    }
    //std::cout << offset << " 2, " << chunk_len - 1 << std::endl;
    threads[threads.size() - 1] = std::make_unique<std::thread>(
            f<vec_type>, threads.size() - 1, ptr + offset, chunk_len - 1, offset % 2, std::ref(barriers));

    for (auto &thread : threads)
        thread->join();
    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

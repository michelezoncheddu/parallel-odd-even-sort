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
    auto swaps{0};
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
void f(T *v, size_t end, barrier &barrier) {
    do {
        barrier.wait(); // needed?
        global_swaps  = odd_even_sort(v, 1, end); // odd
        barrier.wait();
        global_swaps += odd_even_sort(v, 0, end); // even
        barrier.wait();
    } while (global_swaps > 0);
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage is " << argv[0]
                  << " n nw" << std::endl;
        return -1;
    }

    auto const n = strtol(argv[1], nullptr, 10);
    auto const nw = strtol(argv[2], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);
    auto const ptr = v.data();

    barrier barrier(nw);
    std::vector<std::thread*> threads(nw);

    size_t const chunk_len = v.size() / nw;
    int remaining = static_cast<int>(v.size() % nw);
    size_t offset = 0;

    auto const start_time = std::chrono::system_clock::now();
    for (int i = 0; i < nw - 1; ++i) {
        //std::cout << offset << ", " << chunk_len + (remaining > 0) << std::endl;
        threads[i] = new std::thread(f<vec_type>, ptr + offset, chunk_len + (remaining > 0), std::ref(barrier));
        offset += chunk_len + (remaining > 0);
        --remaining;
    }
    //std::cout << offset << ", " << chunk_len - 1 << std::endl;
    threads[threads.size() - 1] = new std::thread(f<vec_type>, ptr + offset, chunk_len - 1, std::ref(barrier));

    for (auto thread : threads)
        thread->join();
    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

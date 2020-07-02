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
    int swaps = 0;
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

int main() {
    auto v = create_random_vector<vec_type>(N_ELEM, MIN, MAX, SEED);
    auto const ptr = v.data();

    int const nw = 2;
    barrier barrier(nw);

    size_t const chunk_len = v.size() / nw;

    std::vector<std::thread*> threads(nw);

    auto const start_time = std::chrono::system_clock::now();
    for (size_t i = 0, offset = 0; i < nw; ++i, offset += chunk_len)
        threads[i] = new std::thread(f<vec_type>, ptr + offset, chunk_len, std::ref(barrier));

    for (auto thread : threads)
        thread->join();
    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

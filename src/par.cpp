#include <algorithm>
#include <atomic>
#include <chrono>
#include <cassert>
#include <iostream>
#include <thread>

#include <config.hpp>
#include <util.hpp>

std::atomic<int> global_swaps = 0;

template <typename T>
int odd_even_sort(T *v, size_t end) {
    int swaps{0};
    for (size_t i = 0; i < end; i += 2) {
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
void f(T *v, size_t end) {
    do {
        global_swaps  = odd_even_sort(v + 1, end); // odd
        global_swaps += odd_even_sort(v, end); // even
    } while (global_swaps > 0);
}

int main() {
    auto v = create_random_vector<vec_type>(100000, MIN, MAX, SEED);
    auto ptr = v.data();

    auto start_time = std::chrono::system_clock::now();
    std::thread t(f<vec_type>, ptr, v.size() - 1);
    t.join();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

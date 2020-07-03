#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

#include <config.hpp>
#include <util.hpp>

template <typename T>
int odd_even_sort(std::vector<T> &v, int start) {
    auto swaps{0};
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

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cout << "Usage is " << argv[0]
                  << " n" << std::endl;
        return -1;
    }

    auto const n = strtol(argv[1], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);
    int swaps;

    auto start = std::chrono::system_clock::now();
    do {
        swaps  = odd_even_sort(v, 1); // odd
        swaps += odd_even_sort(v, 0); // even
    } while (swaps > 0);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

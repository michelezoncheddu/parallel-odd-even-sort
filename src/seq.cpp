#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

using vec_type = int;

template <typename T>
int odd(std::vector<T> &v) {
    auto swaps{0};
    for (size_t i = 1; i < v.size() - 1; i += 2) {
        auto odd = v[i], even = v[i + 1];
        v[i] = odd > even ? even : odd;
        v[i + 1] = odd > even ? odd : even;
        if (v[i] != odd)
            swaps++;
    }
    return swaps;
}

template <typename T>
int even(std::vector<T> &v, int start) {
    auto swaps{0};
    for (size_t i = start; i < v.size() - 1; i += 2) {
        auto even = v[i], odd = v[i + 1];
        v[i] = even > odd ? odd : even;
        v[i + 1] = even > odd ? even : odd;
        if (v[i] != even)
            swaps++;
    }
    return swaps;
}

std::vector<int> create_random_vector(int n, int min, int max, unsigned seed) {
    //std::mt19937 gen{std::random_device{}()};
    std::mt19937 gen{seed};

    std::uniform_int_distribution<> dis(min, max);

    std::vector<int> v(n);
    generate(begin(v), end(v), [&]{ return dis(gen); });
    return v;
}

int main() {
    auto v = create_random_vector(10000, 0, INT32_MAX, 123);

    int swaps;
    auto start = std::chrono::system_clock::now();
    do {
        swaps = 0;
        swaps += odd<vec_type>(v);
        swaps += even<vec_type>(v, 0);
    } while (swaps > 0);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start).count();
    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

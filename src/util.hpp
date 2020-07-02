#ifndef ODD_EVEN_SORT_UTIL_HPP
#define ODD_EVEN_SORT_UTIL_HPP

#include <algorithm>
#include <random>
#include <vector>

template <typename T>
std::vector<T> create_random_vector(size_t n, int min, int max, unsigned seed) {
    //std::mt19937 gen{std::random_device{}()};
    std::mt19937 gen{seed};

    std::uniform_int_distribution<> dis(min, max);

    std::vector<T> v(n);
    generate(begin(v), end(v), [&]{ return dis(gen); });
    return v;
}

#endif // ODD_EVEN_SORT_UTIL_HPP

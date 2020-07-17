/**
 * @file   util.hpp
 * @brief  It implements some utility functions
 * @author Michele Zoncheddu
 */


#ifndef ODD_EVEN_SORT_UTIL_HPP
#define ODD_EVEN_SORT_UTIL_HPP

#include <algorithm> // std::generate
#include <random>
#include <vector>

template <typename T>
std::vector<T> create_random_vector(size_t n, int min, int max, unsigned seed) {
    //std::mt19937 gen{std::random_device{}()};
    std::mt19937 gen{seed};

    std::uniform_int_distribution<> dis(min, max);

    std::vector<T> v(n);
    std::generate(v.begin(), v.end(), [&]{ return dis(gen); });

    return v;
}

#endif // ODD_EVEN_SORT_UTIL_HPP

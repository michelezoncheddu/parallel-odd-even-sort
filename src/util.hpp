/**
 * @file   util.hpp
 * @brief  It contains some utility functions
 * @author Michele Zoncheddu
 */


#ifndef ODD_EVEN_SORT_UTIL_HPP
#define ODD_EVEN_SORT_UTIL_HPP

#include <algorithm> // std::generate
#include <random>
#include <vector>

/**
 * @brief Generates a vector of random numbers.
 *
 * @tparam T the vector type
 * @param n the number of element to put into the vector
 * @param min the lower bound for the values
 * @param max the upper bound for the values
 * @param seed the seed for the random generator
 * @return the vector
 */
template <typename T>
std::vector<T> create_random_vector(size_t n, int min, int max, unsigned seed = std::random_device{}()) {
    std::mt19937 gen{seed};

    std::uniform_real_distribution<> dis(min, max);

    std::vector<T> v(n);
    std::generate(v.begin(), v.end(), [&]{ return dis(gen); });

    return v;
}

#endif // ODD_EVEN_SORT_UTIL_HPP

/**
 * @file   barrier.hpp
 * @brief  It describes an atomic barrier for threads synchronization
 * @author Michele Zoncheddu
 */


#ifndef ODD_EVEN_SORT_BARRIER_HPP
#define ODD_EVEN_SORT_BARRIER_HPP

#include <atomic>

class barrier {
private:
    std::atomic<int> n;

public:
    explicit barrier(int);

    void set(int);

    void dec();

    void wait();

    int read();
};

#endif // ODD_EVEN_SORT_BARRIER_HPP

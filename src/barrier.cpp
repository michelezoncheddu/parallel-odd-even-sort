/**
 * @file   barrier.cpp
 * @brief  It implements an atomic barrier for threads synchronization
 * @author Michele Zoncheddu
 */


#include <barrier.hpp>

barrier::barrier(int n) : n{n} {}

void barrier::set(int _n) {
    n = _n;
}

void barrier::dec() {
    --n;
}

void barrier::wait() {
    --n;
    while (n != 0)
        ;
}

int barrier::read() {
    return n;
}

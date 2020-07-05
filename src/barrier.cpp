#include "barrier.hpp"

barrier::barrier(int n) : n{n} {}

void barrier::set_t(int in) {
    n = in;
}

void barrier::dec() {
    --n;
}

void barrier::stop() {
    while (n != 0)
        ;
}

void barrier::wait() {
    --n;
    while (n != 0)
        ;
}

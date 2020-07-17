#include <algorithm>
#include <atomic>
#include <chrono>
#include <cassert>
#include <iostream>
#include <thread>

#include <barrier.hpp>
#include <config.hpp>
#include <util.hpp>

using type = int;

std::atomic<type> global_swaps = 0;

int n, nw;

template <typename T>
type odd_even_sort(T *v, short phase, size_t end) {
    auto swaps = 0;
    for (size_t i = phase; i < end; i += 2) {
        auto first = v[i] , second = v[i - 1];
        auto cond = first > second;
        v[i] = cond ? second : first;
        v[i + 1] = cond ? first : second;

        /*auto start = std::chrono::high_resolution_clock::now();
        while (!end) {
            auto elapsed = std::chrono::high_resolution_clock::now() - start;
            auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            if (msec > 1)
                end = true;
        }*/

        swaps++;
    }
    return swaps;
}

template <typename T>
void f(unsigned thid, T *v, size_t end, barrier &barrier) {
    std::vector<int> vec(n / nw);
    type odd, even;
    do {
        //barrier.wait();
        //if (thid == 0)
        //   global_swaps = 0;
        odd = odd_even_sort(vec.data(), 1, vec.size() - 1);
        //barrier.wait();
        even = odd_even_sort(vec.data(), 0, vec.size() - 1);
        global_swaps += (odd + even);
        //barrier.wait();
    } while (global_swaps < n * 10000);
//    while (true) {
//        if (thid == 0)
//            global_swaps = 0;
//        bool odd = odd_even_sort(v, 1, end);
//        barrier.wait();
//        bool even = odd_even_sort(v, 0, end);
//        global_swaps |= (odd || even);
//        barrier.wait();
//        if (!odd && !even && !global_swaps)
//            break;
//        barrier.wait();
//    }
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage is " << argv[0]
                  << " n nw" << std::endl;
        return -1;
    }

    n  = strtol(argv[1], nullptr, 10);
    nw = strtol(argv[2], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);
    auto const ptr = v.data();

    barrier barrier(nw);
    std::vector<std::thread*> threads(nw);

    size_t const chunk_len = v.size() / nw;
    int remaining = static_cast<int>(v.size() % nw);
    size_t offset = 0;

    auto const start_time = std::chrono::system_clock::now();
    for (unsigned i = 0; i < nw - 1; ++i) {
//        std::cout << offset << " 1, " << chunk_len + (remaining > 0) << std::endl;
        threads[i] = new std::thread(f<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), std::ref(barrier));
        offset += chunk_len + (remaining > 0);
        --remaining;
    }
//    std::cout << offset << " 2, " << chunk_len - 1 << std::endl;
    threads[threads.size() - 1] = new std::thread(f<vec_type>, threads.size() - 1, ptr + offset, chunk_len - 1, std::ref(barrier));

    for (auto thread : threads)
        thread->join();
    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

//    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

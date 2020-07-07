#include <algorithm>
#include <atomic>
#include <chrono>
#include <cassert>
#include <iostream>
#include <thread>

#include <barrier.hpp>
#include <config.hpp>
#include <util.hpp>

bool finished = false;
unsigned constexpr padding = 64 / sizeof(unsigned);

template <typename T>
int odd_even_sort(T *v, short phase, size_t end) {
    int swaps = 0;
    for (size_t i = phase; i < end; i += 2) {
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
void f(unsigned thid, T *v, size_t end, int alignment, std::vector<std::unique_ptr<barrier>> &barriers1, std::vector<std::unique_ptr<barrier>> &barriers2, std::vector<unsigned> &swaps) {
    int iter = 0;
    int pos = thid * padding;
    while (!finished) {
        swaps[pos] |= (unsigned)odd_even_sort(v, !alignment, end);
        barriers1[iter]->wait();
        swaps[pos] |= (unsigned)odd_even_sort(v, alignment, end);
        barriers2[iter++]->wait();
    }
}

void controller_body(std::vector<std::unique_ptr<barrier>> &barriers, std::vector<unsigned> &swaps) {
    int iter = 0;
    while (true) {
        unsigned tmp = 0;
        while (!tmp && barriers[iter]->read() > 1) {
            for (size_t i = 0; i < swaps.size(); i += padding)
                tmp |= swaps[i];
        }

        if (!tmp) {
            size_t i = 0;
            while (!tmp && i < swaps.size()) {
                tmp |= swaps[i];
                i += padding;
            }
        }

        if (!tmp) {
            finished = true;
            barriers[iter]->dec();
            return;
        }

        for (size_t i = 0; i < swaps.size(); i += padding)
            swaps[i] = 0;
        barriers[iter++]->wait();
    }
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage is " << argv[0]
                  << " n nw" << std::endl;
        return -1;
    }

    auto const n  = strtol(argv[1], nullptr, 10);
    auto const nw = strtol(argv[2], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);
    auto const ptr = v.data();

    //barrier barrier(nw);
    std::vector<std::unique_ptr<barrier>> barriers1, barriers2;
    barriers1.resize(n);
    barriers2.resize(n);
    for (auto &elem : barriers1)
        elem = std::make_unique<barrier>(nw);
    for (auto &elem : barriers2)
        elem = std::make_unique<barrier>(nw + 1);

    std::vector<std::unique_ptr<std::thread>> threads(nw);
    std::vector<unsigned> swaps(nw * padding, 0);

    size_t const chunk_len = v.size() / nw;
    int remaining = static_cast<int>(v.size() % nw);
    size_t offset = 0;

    auto const start_time = std::chrono::system_clock::now();

    std::thread controller(controller_body, std::ref(barriers2), std::ref(swaps));

    for (unsigned i = 0; i < nw - 1; ++i) {
        //std::cout << offset << " 1, " << chunk_len + (remaining > 0) << std::endl;
        threads[i] = std::make_unique<std::thread>(
                f<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), offset % 2, std::ref(barriers1), std::ref(barriers2), std::ref(swaps));
        offset += chunk_len + (remaining > 0);
        --remaining;
    }
    //std::cout << offset << " 2, " << chunk_len - 1 << std::endl;
    threads[nw - 1] = std::make_unique<std::thread>(
            f<vec_type>, nw - 1, ptr + offset, chunk_len - 1, offset % 2, std::ref(barriers1), std::ref(barriers2), std::ref(swaps));

    controller.join();
    for (auto &thread : threads)
        thread->join();
    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    /*for (auto elem : v)
        std::cout << elem << " ";*/

    assert(std::is_sorted(v.begin(), v.end()));

    return 0;
}

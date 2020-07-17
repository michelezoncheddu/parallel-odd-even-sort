#include <algorithm>
#include <atomic>
#include <chrono>
#include <cassert>
#include <iostream>
#include <thread>

#include <barrier.hpp>
#include <config.hpp>
#include <util.hpp>

std::vector<vec_type> shared;
bool finished = false;

unsigned constexpr padding = 64 / sizeof(unsigned);
unsigned constexpr padding_2 = 64 / sizeof(vec_type) / 2;

int nw;

template <typename T>
bool odd_even_sort(T *v, bool phase, size_t end) {
    auto swaps = 0;
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
    std::vector<vec_type> local_vector;
    for (size_t i = 0; i < end; ++i)
        local_vector.push_back(v[i]);

    auto pos = thid * padding;

    auto iter = 0;
    auto const my_end = local_vector.size() - 1;
    auto const has_right_neigh = thid < nw - 1;

    shared[thid * padding_2] = local_vector[0];
    barriers1[iter++]->wait();

    while (!finished) {
        swaps[pos] |= odd_even_sort(local_vector.data(), !alignment, my_end);
        if (has_right_neigh && my_end % 2 != alignment) {
            if (local_vector[my_end] > shared[(thid + 1) * padding_2]) {
                std::swap(local_vector[my_end], shared[(thid + 1) * padding_2]);
                swaps[pos] |= true;
            }
        }

        if (!alignment)
            local_vector[0] = shared[thid * padding_2];
        else
            shared[thid * padding_2] = local_vector[0];

        barriers1[iter]->wait();

        swaps[pos] |= odd_even_sort(local_vector.data(), alignment, my_end);
        if (has_right_neigh && my_end % 2 == alignment) {
            if (local_vector[my_end] > shared[(thid + 1) * padding_2]) {
                std::swap(local_vector[my_end], shared[(thid + 1) * padding_2]);
                swaps[pos] |= true;
            }
        }

        if (alignment)
            local_vector[0] = shared[thid * padding_2];
        else
            shared[thid * padding_2] = local_vector[0];

        barriers2[iter++]->wait();
    }

    for (auto i = 0; i < local_vector.size(); ++i)
        v[i] = local_vector[i];
}

void controller_body(std::vector<std::unique_ptr<barrier>> &barriers, std::vector<unsigned> &swaps) {
    int iter = 1;
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
    nw = strtol(argv[2], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);
    auto const ptr = v.data();

//    for (auto elem : v)
//        std::cout << elem << " ";
//    std::cout << std::endl;

    shared = std::vector<vec_type>(nw * padding_2);

    //barrier barrier(nw);
    std::vector<std::unique_ptr<barrier>> barriers1, barriers2;
    barriers1.resize(n + 1);
    barriers2.resize(n + 1);
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
    threads[threads.size() - 1] = std::make_unique<std::thread>(
            f<vec_type>, threads.size() - 1, ptr + offset, chunk_len, offset % 2, std::ref(barriers1), std::ref(barriers2), std::ref(swaps));

    controller.join();
    for (auto &thread : threads)
        thread->join();
    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

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

std::atomic<int> global_swaps = 0;

unsigned constexpr padding = 64 / sizeof(vec_type) / 2;

int nw;

template <typename T>
int odd_even_sort(T *v, bool phase, size_t end) {
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
void f(unsigned thid, T *v, size_t end, bool alignment, std::vector<std::unique_ptr<barrier>> &barriers) {
    std::vector<vec_type> local_vector;
    for (size_t i = 0; i < end; ++i)
        local_vector.push_back(v[i]);

    auto iter = 0;
    auto const my_end = local_vector.size() - 1;
    auto const has_right_neigh = thid < nw - 1;

    shared[thid * padding] = local_vector[0];
    barriers[iter++]->wait();

    while (true) {
        if (thid == 0)
            global_swaps = 0;

        bool odd = odd_even_sort(local_vector.data(), !alignment, my_end);
        if (has_right_neigh && my_end % 2 != alignment) {
            if (local_vector[my_end] > shared[(thid + 1) * padding]) {
                std::swap(local_vector[my_end], shared[(thid + 1) * padding]);
                odd = true;
            }
        }

        if (!alignment)
            local_vector[0] = shared[thid * padding];
        else
            shared[thid * padding] = local_vector[0];

        barriers[iter++]->wait();

        bool even = odd_even_sort(local_vector.data(), alignment, my_end);
        if (has_right_neigh && my_end % 2 == alignment) {
            if (local_vector[my_end] > shared[(thid + 1) * padding]) {
                std::swap(local_vector[my_end], shared[(thid + 1) * padding]);
                even = true;
            }
        }

        if (alignment)
            local_vector[0] = shared[thid * padding];
        else
            shared[thid * padding] = local_vector[0];

        global_swaps |= (odd || even);
        barriers[iter++]->wait();

        if (!odd && !even && !global_swaps)
            break;
        barriers[iter++]->wait();
    }

    for (auto i = 0; i < local_vector.size(); ++i)
        v[i] = local_vector[i];

//    std::this_thread::sleep_for(std::chrono::milliseconds(100 * thid));
//    for (auto elem : local_vector)
//        std::cout << elem << " ";
//    std::cout << std::endl;
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

    shared = std::vector<vec_type>(nw * padding);

    //barrier barrier(nw);
    std::vector<std::unique_ptr<barrier>> barriers;
    barriers.resize(n * 4);
    for (auto &elem : barriers)
        elem = std::make_unique<barrier>(nw);

    std::vector<std::unique_ptr<std::thread>> threads(nw);

    size_t const chunk_len = v.size() / nw;
    int remaining = static_cast<int>(v.size() % nw);
    size_t offset = 0;

    auto const start_time = std::chrono::system_clock::now();
    for (unsigned i = 0; i < nw - 1; ++i) {
        //std::cout << offset << " 1, " << chunk_len + (remaining > 0) << std::endl;
        threads[i] = std::make_unique<std::thread>(
                f<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), offset % 2, std::ref(barriers));
        offset += chunk_len + (remaining > 0);
        --remaining;
    }
    //std::cout << offset << " 2, " << chunk_len - 1 << std::endl;
    threads[threads.size() - 1] = std::make_unique<std::thread>(
            f<vec_type>, threads.size() - 1, ptr + offset, chunk_len, offset % 2, std::ref(barriers));

    for (auto &thread : threads)
        thread->join();
    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));
    return 0;
}

#include <algorithm> // For is_sorted
#include <cassert>
#include <iostream>
#include <memory> // For smart pointers
#include <thread>
#include <vector>

#include <config.hpp>
#include <util.hpp>

#include <ff/ff.hpp>
#include <ff/farm.hpp>

using namespace ff;

template <typename T>
unsigned odd_even_sort(T * const v, short phase, size_t end) {
    unsigned swaps = 0;
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

struct Emitter : ff_monode_t<int> {
    Emitter(std::vector<vec_type> &v, unsigned nw) : v{v}, nw{nw} {}
    ~Emitter() override = default;

    int* svc(int *task) override {
        std::cout << v.size() << std::endl;
    }

    std::vector<vec_type> &v;
    unsigned const nw;
};

struct Worker : ff_node_t<int> {
    Worker() {}
    ~Worker() override = default;

    int* svc(int *task) override {
        return nullptr;
    }
};

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage is " << argv[0]
                  << " n nw" << std::endl;
        return -1;
    }

    auto const n  = strtol(argv[1], nullptr, 10); // Array length
    auto const nw = strtol(argv[2], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);

    auto const start_time = std::chrono::system_clock::now();

    Emitter emitter(v, nw);
    ff_Farm<> farm([&]() {
           std::vector<std::unique_ptr<ff_node>> workers(nw);
           for (auto &elem : workers)
               elem = make_unique<Worker>();
           return workers;
        } (),
        emitter);
    farm.remove_collector();
    farm.wrap_around();
    if (farm.run_and_wait_end() < 0) {
        error("running farm");
        return EXIT_FAILURE;
    }

//    size_t const chunk_len = v.size() / nw;
//    int remaining = static_cast<int>(v.size() % nw);
//    size_t offset = 0;
//
//    for (unsigned i = 0; i < nw - 1; ++i) {
//        threads.push_back(std::make_unique<std::thread>(
//                thread_body<vec_type>, i, ptr + offset, chunk_len + (remaining > 0), offset % 2,
//                std::ref(phases), std::ref(swaps), std::ref(barriers)));
//        offset += chunk_len + (remaining > 0);
//        --remaining;
//    }
//    threads.push_back(std::make_unique<std::thread>(
//            thread_body<vec_type>, nw - 1, ptr + offset, chunk_len - 1, offset % 2,
//            std::ref(phases), std::ref(swaps), std::ref(barriers)));

    auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time).count();

    std::cout << duration << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));

    return 0;
}

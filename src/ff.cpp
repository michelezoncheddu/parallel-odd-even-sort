/**
 * @file   ff.cpp
 * @brief  FastFlow parallel implementation of the odd-even sort algorithm
 * @author Michele Zoncheddu
 */


#include <algorithm> // std::is_sorted
#include <cassert>
#include <iostream>
#include <memory>    // Smart pointers
#include <vector>

#include <config.hpp>
#include <util.hpp>

#include <ff/ff.hpp>
#include <ff/farm.hpp>

using namespace ff;

/**
 * @brief It performs an odd or an even sorting phase on the array
 *
 * @tparam T the vector pointer type
 * @param v the pointer to the vector
 * @param phase the phase (odd or even)
 * @param end the end of the array
 * @return the number of swaps
 */
template <typename T>
unsigned odd_even_sort(T * const v, short const phase, size_t const end) {
    unsigned swaps = 0;
    for (size_t i = phase; i < end; i += 2) {
        auto first = v[i], second = v[i + 1];
        auto cond = first > second;
        v[i]     = cond ? second : first;
        v[i + 1] = cond ? first : second;
        if (v[i] != first)
            swaps++;
    }
    return swaps;
}

struct Emitter : ff_monode_t<unsigned> {
    explicit Emitter(int nw) : nw{nw} {}

    unsigned* svc(unsigned *task) override {
        static unsigned remaining = 1; // The task that starts the emitter
        static unsigned swaps = 1;
        static unsigned dummy_task = 0;
        static bool previous_zero = false;

        // task always from feedback
        if (!swaps)
            swaps = *task;

        if (--remaining == 0) {
            if (previous_zero && !swaps) // Zero swaps also in the previous phase, stop
                return EOS;
            broadcast_task(&dummy_task);
            previous_zero = swaps == 0;
            swaps = 0;
            remaining = nw;
        }
        return GO_ON;
    }

    int const nw;
};

struct Worker : ff_node_t<unsigned> {
    Worker(vec_type * const v, size_t const end, short alignment) : v{v}, end{end}, alignment{alignment} {}

    unsigned* svc(unsigned *) override {
        swaps = odd_even_sort(v, alignment, end);

        alignment = !alignment; // Change phase

        /*
         * I can send my local variable because it goes only to the emitter,
         * and the next svc call of this worker will happen only after that the emitter read the value.
         */
        return &swaps;
    }

    vec_type * const v;
    size_t const end;
    short alignment;

    unsigned swaps = 0;
};

/**
 * @brief the starting method
 *
 * @return the exit status
 */
int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage is " << argv[0]
                  << " n nw [seed]" << std::endl;
        return -1;
    }

    auto const n  = strtol(argv[1], nullptr, 10); // Array length
    auto const nw = strtol(argv[2], nullptr, 10);

    if (n < 1 || nw < 1) {
        std::cout << "n and nw must be greater than zero" << std::endl;
        return -1;
    }

    if (n < nw) {
        std::cout << "nw must be greater than n" << std::endl;
        return -1;
    }

    // Create the vector
    std::vector<vec_type> v;
    if (argc > 3)
        v = create_random_vector<vec_type>(n, MIN, MAX, strtol(argv[3], nullptr, 10));
    else
        v = create_random_vector<vec_type>(n, MIN, MAX);

    ffTime(START_TIME);
    Emitter emitter(nw);
    ff_Farm<> farm([&]() {
                   std::vector<std::unique_ptr<ff_node>> workers;
                   auto const ptr = v.data();
                   size_t const chunk_len = v.size() / nw;
                   long remaining = static_cast<long>(v.size() % nw);
                   size_t offset = 0;

                   for (unsigned i = 0; i < nw - 1; ++i) {
                       workers.push_back(make_unique<Worker>(
                               ptr + offset, chunk_len + (remaining > 0), offset % 2));
                       offset += chunk_len + (remaining > 0);
                       --remaining;
                   }
                   workers.push_back(make_unique<Worker>(
                           ptr + offset, chunk_len - 1, offset % 2));
                   return workers;
               } (),
               emitter);
    farm.remove_collector();
    farm.wrap_around();
    if (farm.run_and_wait_end() < 0) {
        error("running farm");
        return EXIT_FAILURE;
    }
    ffTime(STOP_TIME);

    std::cout << "Time: " << ffTime(GET_TIME) << " ms" << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));

    return 0;
}

#include <algorithm> // For is_sorted
#include <cassert>
#include <iostream>
#include <memory> // For smart pointers
#include <vector>

#include <config.hpp>
#include <util.hpp>

#include <ff/ff.hpp>
#include <ff/farm.hpp>

using namespace ff;

auto const padding = cache_line_size() / sizeof(unsigned);

unsigned nw;

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

struct Emitter : ff_monode_t<std::pair<unsigned, unsigned>, unsigned> {
    Emitter() {
        remaining = nw;
    }
    ~Emitter() override = default;

    unsigned* svc(std::pair<unsigned, unsigned> *task) override {
        // Initial phase
        if (task == nullptr) {
            broadcast_task(&RUN);
            return GO_ON;
        }

        // task from feedback

//        if (task->second != iteration)
//            return GO_ON;

        if (!swaps)
            swaps |= task->first;

//        if (swaps) {
//            broadcast_task(&RUN);
//            swaps = 0;
//            remaining = nw;
//            iteration++;
//            return GO_ON;
//        }

        if (--remaining == 0) {
            if (!swaps) {
                return EOS;
            } else {
                broadcast_task(&RUN);
                swaps = 0;
                remaining = nw;
                iteration++;
            }
        }
        return GO_ON;
    }

    unsigned remaining;

    unsigned swaps = 0;
    unsigned iteration = 1;

    unsigned RUN = 1; // Dummy task
};

struct Worker : ff_node_t<unsigned, std::pair<unsigned, unsigned>> {
    Worker(unsigned thid, vec_type * const v, size_t end, short alignment, std::vector<unsigned> &phases)
        : thid{thid}, v{v}, end{end}, alignment{alignment}, phases{phases} {
        pos = thid * padding;
        has_left_neigh = thid > 0;
        has_right_neigh = thid < nw - 1;
    }

    std::pair<unsigned, unsigned>* svc(unsigned *) override {
        swaps.first = odd_even_sort(v, !alignment, end);

        phases[pos]++; // Ready for the next phase

        // Wait my neighbours to be ready
        if (has_right_neigh)
            while (phases[pos] != phases[(thid + 1) * padding])
                __asm__("nop"); // To force the compiler to don't "optimize" this loop
        if (has_left_neigh)
            while (phases[pos] != phases[(thid - 1) * padding])
                __asm__("nop");

        swaps.first |= odd_even_sort(v, alignment, end);

        swaps.second++;

        return &swaps;
    }

    unsigned thid;
    vec_type * const v;
    size_t end;
    short alignment;
    std::vector<unsigned> &phases;
    unsigned pos;
    bool has_left_neigh, has_right_neigh;

    std::pair<unsigned, unsigned> swaps{0, 0};
};

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        std::cout << "Usage is " << argv[0]
                  << " n nw" << std::endl;
        return -1;
    }

    auto const n = strtol(argv[1], nullptr, 10); // Array length
    nw = strtol(argv[2], nullptr, 10);

    auto v = create_random_vector<vec_type>(n, MIN, MAX, SEED);

    ffTime(START_TIME);

    std::vector<unsigned> phases(nw * padding, 0);

    Emitter emitter;
    ff_Farm<> farm([&]() {
                   std::vector<std::unique_ptr<ff_node>> workers;
                   auto const ptr = v.data();
                   size_t const chunk_len = v.size() / nw;
                   int remaining = static_cast<int>(v.size() % nw);
                   size_t offset = 0;

                   for (unsigned i = 0; i < nw - 1; ++i) {
                      workers.push_back(make_unique<Worker>(
                              i, ptr + offset, chunk_len + (remaining > 0), offset % 2, phases));
                      offset += chunk_len + (remaining > 0);
                      --remaining;
                   }
                   workers.push_back(make_unique<Worker>(
                          nw - 1, ptr + offset, chunk_len - 1, offset % 2, phases));
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

    std::cout << "Time: " << ffTime(GET_TIME) << " (ms)" << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));

    return 0;
}

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

struct Emitter : ff_monode_t<unsigned> {
    explicit Emitter(unsigned nw) : nw{nw}, remaining{nw} {
        phase = std::vector<short>(nw, 0);
    }
    ~Emitter() override = default;

    [[nodiscard]] bool are_neighbors_ready(unsigned i) const {
        auto const has_left_neigh = i > 0, has_right_neigh = i < nw - 1;
        auto res = true;
        if (has_right_neigh)
            res &= (phase[i] <= phase[i + 1]);
        if (has_left_neigh)
            res &= (phase[i] <= phase[i - 1]);
        return res;
    }

    unsigned* svc(unsigned *task) override {
        // Initial phase
        if (task == nullptr) {
            broadcast_task(&RUN);
            return GO_ON;
        }

        /*
         * Phase 0: running odd phase
         * Phase 1: ready for even phase
         * Phase 2: running even phase
         */

        // task from feedback

        auto const worker = get_channel_id();

        if (!swaps)
            swaps = *task;

        // Phase can only be 0 or 2

        if (phase[worker] == 2) { // The worker has finished
            --remaining;
            if (!remaining) {
                if (!swaps)
                    return EOS;
                broadcast_task(&RUN);
                swaps = 0;
                remaining = nw;
                for (auto &elem : phase)
                    elem = 0;
            }
            return GO_ON;
        }

        // Phase is 0
        // Unlock workers
        phase[worker]++;
        for (unsigned i = 0; i < nw; ++i) {
            if (phase[i] == 1 && are_neighbors_ready(i)) {
                ff_send_out_to(&RUN, i);
                phase[i]++;
            }
        }
        return GO_ON;
    }

    unsigned const nw;
    unsigned remaining;
    std::vector<short> phase;

    unsigned swaps = 0; // Number of swaps of the current higher iteration

    unsigned RUN = 1; // Dummy task
};

struct Worker : ff_node_t<unsigned> {
    Worker(vec_type * const v, size_t end, short alignment) : v{v}, end{end}, alignment{alignment} {}

    unsigned* svc(unsigned *) override {
        swaps = odd_even_sort(v, alignment, end);
        alignment = !alignment;
        return &swaps;
    }

    vec_type * const v;
    size_t end;
    short alignment;

    unsigned swaps = 0;
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

    ffTime(START_TIME);

    Emitter emitter(nw);
    ff_Farm<> farm([&]() {
                   std::vector<std::unique_ptr<ff_node>> workers;
                   auto const ptr = v.data();
                   size_t const chunk_len = v.size() / nw;
                   int remaining = static_cast<int>(v.size() % nw);
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

    std::cout << "Time: " << ffTime(GET_TIME) << " (ms)" << std::endl;

    assert(std::is_sorted(v.begin(), v.end()));

    return 0;
}
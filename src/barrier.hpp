#ifndef ODD_EVEN_SORT_BARRIER_HPP
#define ODD_EVEN_SORT_BARRIER_HPP

#include <atomic>

class barrier {
   private:
    unsigned const count;
    std::atomic<unsigned> remaining;
    std::atomic<unsigned> generation;

   public:
    explicit barrier(unsigned count) : count{count}, remaining{count}, generation{0} {}

    void wait() {
        unsigned const my_generation = generation;
        if (!--remaining) {
            remaining = count;
            ++generation;
        } else {
            while (generation == my_generation)
                ; // std::this_thread::yield();
        }
    }
};

#endif // ODD_EVEN_SORT_BARRIER_HPP

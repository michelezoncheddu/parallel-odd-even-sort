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

class abar {
   private:
    std::atomic<int> n;

   public:
    explicit abar(int n) : n{n} {}

    void set_t(int in) {
        n = in;
    }

    void dec() {
        --n;
    }

    void barrier() {
        while (n != 0)
            ;
    }

    void wait() {
        --n;
        while (n != 0)
            ;
    }
};

class barrier_new {
   private:
    int P; // Num threads
    std::atomic<int> bar = 0; // Counter of threads, faced barrier.
    std::atomic<int> passed = 0; // Number of barriers, passed by all threads.

   public:
    explicit barrier_new(int n) : P{n} {}

    void wait() {
        int passed_old = passed.load(std::memory_order_relaxed);

        if (bar.fetch_add(1) == (P - 1)) {
            // The last thread, faced barrier.
            bar = 0;
            // Synchronize and store in one operation.
            passed.store(passed_old + 1, std::memory_order_release);
        } else {
            // Not the last thread. Wait others.
            while (passed.load(std::memory_order_relaxed) == passed_old) {};
            // Need to synchronize cache with other threads, passed barrier.
            std::atomic_thread_fence(std::memory_order_acquire);
        }
    }
};

#endif // ODD_EVEN_SORT_BARRIER_HPP

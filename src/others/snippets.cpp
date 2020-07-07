// reusable barrier.hpp
/*class barrier {
   private:
    unsigned const count;
    std::atomic<unsigned> remaining;
    std::atomic<unsigned> generation;

   public:
    explicit barrier(unsigned count);

    void wait();
    unsigned read();
};
// reusable barrier.cpp
/*barrier::barrier(unsigned count) : count{count}, remaining{count}, generation{0} {}

void barrier::wait() {
    unsigned const my_generation = generation;
    if (!--remaining) {
        remaining = count;
        ++generation;
    } else {
        while (generation == my_generation)
            ; // std::this_thread::yield();
    }
}*/



// passive barrier
/*class Barrier
{
private:
    std::mutex m_mtx;
    std::condition_variable m_cv;
    unsigned m_count;

public:
    /// <summary>Ctor. Initializes a Barrier with count.</summary>
    explicit Barrier(unsigned initialCount);

    /// <summary>
    /// Block the calling thread until the internal count reaches the value zero. Then all waiting threads are unblocked.
    /// </summary>
    void wait();
};

Barrier::Barrier(const unsigned initialCount)
        : m_count(initialCount)
{ }

void Barrier::wait()
{
    std::unique_lock<std::mutex> mtxLock(m_mtx);      // Must use unique_lock with condition variable.
    --m_count;
    if (0 == m_count)
    {
        m_cv.notify_all();
    }
    else
    {
        m_cv.wait(mtxLock, [this]{ return 0 == m_count; });      // Blocking till count is zero.
    }
}*/



/*class barrier_new {
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
};*/

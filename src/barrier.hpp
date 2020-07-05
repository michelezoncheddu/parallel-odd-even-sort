#ifndef ODD_EVEN_SORT_BARRIER_HPP
#define ODD_EVEN_SORT_BARRIER_HPP

#include <atomic>

class barrier {
   private:
    std::atomic<int> n;

   public:
    explicit barrier(int n);

    void set_t(int in);

    void dec();

    void stop();

    void wait();
};

#endif // ODD_EVEN_SORT_BARRIER_HPP

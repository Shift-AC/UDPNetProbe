#ifndef __SEMAPHORE_HH__
#define __SEMAPHORE_HH__

#include <condition_variable>
#include <chrono>
#include <mutex>

// code from http://blog.csdn.net/dongzhiliu/article/details/76083905
class Semaphore {
public:
    Semaphore(long count = 0): mutex_(), cv_(), count_(count) {}

    void release() 
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++count_;
        cv_.notify_one();
    }

    void issue() 
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return count_ > 0; });
        --count_;
    }

    bool tryIssue(long us)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        std::chrono::microseconds time(us);
        if (cv_.wait_for(lock, time, [&] { return count_ > 0; }))
        {
            --count_;
            return true;
        }
        return false;
    }

    long get() 
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return count_;
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (count_ > 0)
        {
            count_ = 0;
        }
    }
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    long count_;
};


#endif

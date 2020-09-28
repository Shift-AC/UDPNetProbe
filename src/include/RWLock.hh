#ifndef __RWLOCK_HH__
#define __RWLOCK_HH__

#include <atomic>

// Solution for writer-first Reader-Writer problem
class RWLock
{
public:
    // reader in process
    std::atomic<unsigned short> readerCount;
    // writer not finished(not necessarily in process, maybe pending)
    std::atomic<unsigned short> writerCount;
    std::atomic_flag writeInProcess;
public:
    RWLock(): readerCount(0), writerCount(0), 
        writeInProcess(ATOMIC_FLAG_INIT) {}

    inline void init()
    {
        readerCount = 0;
        writerCount = 0;
    }

    inline void readLock()
    {
readLock_in:
        while (writerCount.load() != 0);
        readerCount++;
        if (writerCount.load() != 0)
        {
            readerCount--;
            goto readLock_in;
        }
    }
    inline void readRelease()
    {
        readerCount--;
    }
    inline int tryReadLock()
    {
        if (writerCount.load() != 0) return 1;
        readerCount++;
        if (writerCount.load() != 0)
        {
            readerCount--;
            return 1;
        }
        return 0;
    }
    inline void writeLock()
    {
        writerCount++;
        while (readerCount.load() != 0);

        while (writeInProcess.test_and_set(std::memory_order_acquire));
    }
    inline void writeRelease()
    {
        writeInProcess.clear(std::memory_order_release);
        writerCount--;
    }
    inline int tryWriteLock()
    {
        writerCount++;
        if (readerCount.load() != 0)
        {
            writerCount--;
            return 1;
        }
        if (writeInProcess.test_and_set(std::memory_order_acquire))
        {
            writerCount--;
            return 2;
        }
        return 0;
    }
};

#endif

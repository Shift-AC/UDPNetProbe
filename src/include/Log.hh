#ifndef __LOG_HH__
#define __LOG_HH__

#include <string.h>
#include <time.h>
#include <unistd.h>

#include <list>
#include <thread>
#include <vector>

#include "Represent.hh"
#include "RWLock.hh"
#include "Semaphore.hh"

// Asynchronous logger for hsrvdn
// this class uses custom memory allocator to support high-performance &
// non-blocking logging.
// to support fast memory allocation, this class uses pre-allocated fixed-size
// buffers. specifically, we manage our buffer as the following code shows:
//
// char buf[1 << LOGBUF_LEVEL][LOGBUF_LEN];
// std::atomic<int> num;
// char* getbuf()
// {
//     int mask = (1 << LOGBUF_LEVEL) - 1);
//     return buf[num++ & mask];
// }
//
// it is obvious that we must maintain enough number of buffers, or conflicts
// would happen when load is heavy. the problem is, `LOGBUF_LEN` should be as
// large as possible to hold arbitrary user message, but this would not be
// memory-efficient when `LOGBUF_LEVEL` is large. 
// in this class, we use two kinds of buffers(`short`/`long` buffer) for
// different purposes. `short` or default buffer is designed for users to print 
// messages. in default configuration, we use a quite large `LOGBUF_LEVEL` 
// value for it to survive high load. `long` buffer is designed for users to 
// dump large objects(e.g. BakedException/RawException). we use a small 
// `LOGBUF_LEVEL` value for it to reduce memory usage.
class Log
{
private:
    static const int TYPE_LEN = 11;
    static const int TID_LEN = 7;
    static const int TIMESTAMP_LEN = 14;
    static const int PREFIX_LEN = TYPE_LEN + TID_LEN + TIMESTAMP_LEN + 2;
    static const char PREFIX_FMT[];

    class FixSizeMemoryPool
    {
    private:
        Smart<char[]> buf;
        std::atomic<int> seq;
    public:
        const int len;
        const int count;
        FixSizeMemoryPool(int lenLevel, int countLevel);
        char* get();
    };

    class Worker : public std::thread
    {
    private:
        static void main(Log *log);
    public:
        Worker(Log *log): std::thread(main, log) {};
    };
    
    int fd;
    int verbose;
    UniqueSmart<Worker> worker;
    FixSizeMemoryPool shortBuf;
    FixSizeMemoryPool longBuf;
    RWLock pendLock;
    std::list<std::pair<char*, int>> pending;
    Semaphore sem;
    timespec tst;

    using PrefixString = char[PREFIX_LEN - 1];

    RWLock prefixLock;
    std::vector<PrefixString> prefixes;

    template <typename ... Args>
    void issue(char *buf, int len, 
        const char *prefix, const char *fmt, Args&& ... args);

    inline double getTimestamp()
    {
        struct timespec ted;
        clock_gettime(CLOCK_REALTIME_COARSE, &ted);
        return (ted.tv_sec - tst.tv_sec) + 
            (ted.tv_nsec - tst.tv_nsec) / 1000000000.0;
    }
    inline unsigned gettid()
    {
        // OK for c++11, maybe BUGGY in later c++ standards.
    #ifdef SYS_gettid
        return (unsigned)gettid();
    #else
        #warning \
            "SYS_gettid unavailable on this system, using pthread_self instead"
        return (unsigned)pthread_self();
    #endif
    }
    template <typename ... Args>
    inline void shortLog(const char *prefix, const char *fmt, Args&& ... args)
    {
        issue(shortBuf.get(), shortBuf.len,
            prefix, fmt, std::forward<Args>(args) ...);
    }
    
    template <typename ... Args>
    inline void longLog(const char *prefix, const char *fmt, Args&& ... args)
    {
        issue(longBuf.get(), longBuf.len,
            prefix, fmt, std::forward<Args>(args) ...);
    }
public:
    inline int addPrefix(const char* str)
    {
        PrefixString tstr;
        prefixLock.writeLock();
        int prefixNum = prefixes.size();
        prefixes.push_back(tstr);
        strncpy(prefixes[prefixNum], str, PREFIX_LEN - 2);
        prefixes[prefixNum][PREFIX_LEN - 2] = 0;
        prefixLock.writeRelease();

        return prefixNum;
    }
    inline const char* getPrefix(int num)
    {
        const char* prefix;
        prefixLock.readLock();
        prefix = prefixes.at(num);
        prefixLock.readRelease();

        return prefix;
    }
    template <typename ... Args>
    inline void shortLog(int prefixNum, const char *fmt, Args&& ... args)
    {
        issue(shortBuf.get(), shortBuf.len,
            getPrefix(prefixNum), fmt, std::forward<Args>(args) ...);
    }
    template <typename ... Args>
    inline void longLog(int prefixNum, const char *fmt, Args&& ... args)
    {
        issue(longBuf.get(), longBuf.len,
            getPrefix(prefixNum), fmt, std::forward<Args>(args) ...);
    }

    template <typename ... Args>
    inline void message(const char *fmt, Args&& ... args)
    {
        shortLog(" Message ", fmt, std::forward<Args>(args) ...);
    }
    template <typename ... Args>
    inline void longMessage(const char *fmt, Args&& ... args)
    {
        longLog(" Message ", fmt, std::forward<Args>(args) ...);
    }
    
    template <typename ... Args>
    inline void warning(const char *fmt, Args&& ... args)
    {
        shortLog(" Warning ", fmt, std::forward<Args>(args) ...);
    }
    template <typename ... Args>
    inline void longWarning(const char *fmt, Args&& ... args)
    {
        longLog(" Warning ", fmt, std::forward<Args>(args) ...);
    }
    
    template <typename ... Args>
    inline void error(const char *fmt, Args&& ... args)
    {
        shortLog("  Error  ", fmt, std::forward<Args>(args) ...);
    }
    template <typename ... Args>
    inline void longError(const char *fmt, Args&& ... args)
    {
        longLog("  Error  ", fmt, std::forward<Args>(args) ...);
    }
    //inline void exception(const Exception &e)
    //{
    //    longError("Exception: %d, %s", e.geterrcode(), e.geterrmsg());
    //}
    
    template <typename ... Args>
    inline void verbose(int level, const char *fmt, Args&& ... args)
    {
        if (verbose >= level)
        {
            shortLog(" Verbose ", fmt, std::forward<Args>(args) ...);
        }
    }
    template <typename ... Args>
    inline void longVerbose(int level, const char *fmt, Args&& ... args)
    {
        if (verbose >= level)
        {
            longLog(" Verbose ", fmt, std::forward<Args>(args) ...);
        }
    }

    static const int DEF_SHORT_BUF_LEN_LEVEL = 9;
    static const int DEF_SHORT_BUF_CNT_LEVEL = 12;
    static const int DEF_LONG_BUF_LEN_LEVEL = 13;
    static const int DEF_LONG_BUF_CNT_LEVEL = 8;

    Log(
        int fd = STDERR_FILENO, 
        int verbose = 0,
        int shortBufLenLevel = DEF_SHORT_BUF_LEN_LEVEL, 
        int shortBufCountLevel = DEF_SHORT_BUF_CNT_LEVEL, 
        int longBufLenLevel = DEF_LONG_BUF_LEN_LEVEL, 
        int longBufCountLevel = DEF_LONG_BUF_CNT_LEVEL);

    Log(const Log&) = delete;

    inline void bind(int fd)
    {
        this->fd = fd;
    }

    static char *strerror(char *buf)
    {
        int num = errno;
        sprintf(buf, "%d ", num);
        strerror_r(num, buf + strlen(buf), 128);
        return buf;
    }
};

// the default logger
extern Log log;

#endif

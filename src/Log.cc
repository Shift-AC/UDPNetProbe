#include <memory>

#include "Log.hh"

Log log;

const char Log::PREFIX_FMT[] = "[%9s]{%5u}(%14.6lf): ";

Log::FixSizeMemoryPool::FixSizeMemoryPool(int lenLevel, int countLevel): 
    buf(), seq(0), len(1 << lenLevel),
    count(1 << countLevel) 
{
    buf = std::make_unique<char[]>(1 << (lenLevel + countLevel));
}

char* Log::FixSizeMemoryPool::get()
{
    int pos = seq++ & (count - 1);
    return buf.get() + (pos * len);
}

void Log::Worker::main(Log *log)
{
    while (1)
    {
        log->sem.issue();

        do
        {
            log->pendLock.writeLock();
            auto item = log->pending.front();
            log->pending.pop_front();
            log->pendLock.writeRelease();

            int nleft = item.second;
            char *bufp = item.first;
            int nwritten;

            while (nleft > 0)
            {
                if ((nwritten = write(log->fd, item.first, nleft)) 
                    <= 0)
                {
                    if (errno == EINTR || 
                        errno == EAGAIN || 
                        errno == EWOULDBLOCK)
                    {    
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                nleft -= nwritten;
                bufp += nwritten;
            }
        }
        while (log->sem.tryIssue(0));
    }
}

Log::Log(int fd, int verboseLevel, int shortBufLenLevel, 
    int shortBufCountLevel, int longBufLenLevel, int longBufCountLevel): 
    fd(fd), verboseLevel(verboseLevel), 
    worker(nullptr), shortBuf(shortBufLenLevel, shortBufCountLevel),
    longBuf(longBufLenLevel, longBufCountLevel), pendLock(), pending(), sem(0),
    prefixLock(), prefixes()
{
    clock_gettime(CLOCK_REALTIME_COARSE, &tst);
    
    time_t rawtime = tst.tv_sec;
    struct tm timeinfo;
    char timeStr[80];

    localtime_r(&rawtime, &timeinfo);
    strftime(timeStr, 80, "%z %Y-%m-%d %H:%M:%S", &timeinfo);

    message("Logger initialized at %s(%lf)", timeStr,
               tst.tv_sec + tst.tv_nsec / (double)1000000000.0);

    worker = std::make_unique<Worker>(this);
}

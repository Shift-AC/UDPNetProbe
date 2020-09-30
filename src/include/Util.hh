#ifndef __UTIL_HH__
#define __UTIL_HH__

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef VERSION 
#define VERSION "Undefined"
#endif

const int PAK_SIZE = 1400;

enum MessageType
{
    RAW,
    DATA,
    ACK,
    INSTRUCTION
};

enum Instructions
{
    START,
    STOP
};

struct Message
{
    MessageType type;
    long value;
};

class CompactRecorder
{
    int fd;
public:
    enum Type
    {
        SENT,
        RECEIVED,
        ACK_SENT,
        ACKED
    };

    struct Record
    {
        long pakSeq;
        Type type;
        int nanosec;
        long sec;
    };

    inline CompactRecorder(): fd(-1) {}

    int init(const char *path)
    {
        int len = strlen(path);
        if (len < 1)
        {
            log.error("CompactRecorder::init: Empty output file name.");
            return 1;
        }
        
        if (path[0] == '-' && path[1] == 0)
        {
            fd = STDOUT_FILENO;
        }
        else
        {
            fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0644);

            if (fd == -1)
            {
                char errbuf[64];
                log.error("CompactRecorder::init: "
                    "Cannot open file %s for output(%s).", 
                    path, Log::strerror(errbuf));
                return 2;
            }
        }

        return 0;
    }

    int write(long pakSeq, Type type)
    {
        if (fd == -1)
        {
            return -1;
        }

        struct timespec current;
        clock_gettime(CLOCK_REALTIME_COARSE, &current);

        Record rec = {pakSeq, type, (int)current.tv_nsec, current.tv_sec};

        return ::write(fd, &rec, sizeof(Record)) < 0;
    }
};

#endif

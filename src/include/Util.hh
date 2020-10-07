#ifndef __UTIL_HH__
#define __UTIL_HH__

#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef VERSION 
#define VERSION "Undefined"
#endif

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
        ACKED,
        IGNORED
    };

    struct Record
    {
        long pakSeq;
        int type;
        int nanosec;
        long sec;
    };

    inline CompactRecorder(): fd(-1) {}

    int init(const char *path);

    int write(long pakSeq, Type type);
};

class RecordReader
{
    int fd;
public:
    inline RecordReader(): fd(-1) {}

    int init(const char *path);
    int next(CompactRecorder::Record &res);
};

typedef void (*sighandler)(int, siginfo_t*, void*);
sighandler signalNoRestart(int signum, sighandler handler);

#endif

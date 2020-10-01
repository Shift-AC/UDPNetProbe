#include "Log.hh"
#include "Util.hh"

sighandler signalNoRestart(int signum, sighandler handler)
{
    struct sigaction action, oldAction;
    char errbuf[256];

    action.sa_sigaction = handler; 
    // block all signal(except SIGTERM) here. We use SIGTERM instead of SIGKILL
    // as a final way of closing a program.
    sigfillset(&action.sa_mask);
    sigdelset(&action.sa_mask, SIGTERM);
    action.sa_flags = 0;

    if (sigaction(signum, &action, &oldAction) < 0)
    {
        log.error("signalNoRestart: sigaction() failed(%s).", 
            Log::strerror(errbuf));
        exit(1);
    }
    return oldAction.sa_sigaction;
}

int CompactRecorder::init(const char *path)
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

int CompactRecorder::write(long pakSeq, Type type)
{
    if (fd == -1)
    {
        return -1;
    }

    struct timespec current;
    clock_gettime(CLOCK_REALTIME_COARSE, &current);

    Record rec = {pakSeq, type, (int)current.tv_nsec, current.tv_sec};

    // we rely on the fact that write() is atomic after Linux 3.14
    return ::write(fd, &rec, sizeof(Record)) < 0;
}

int RecordReader::init(const char *path)
{
    int len = strlen(path);
    if (len < 1)
    {
        log.error("RecordReader::init: Empty input file name.");
        return 1;
    }
    
    if (path[0] == '-' && path[1] == 0)
    {
        fd = STDIN_FILENO;
    }
    else
    {
        fd = open(path, O_RDONLY);

        if (fd == -1)
        {
            char errbuf[64];
            log.error("RecordReader::init: "
                "Cannot open file %s for input(%s).", 
                path, Log::strerror(errbuf));
            return 2;
        }
    }

    return 0;
}

int RecordReader::next(CompactRecorder::Record &res)
{
    if (read(fd, &res.pakSeq, sizeof(long)) < sizeof(long))
    {
        return 1;
    }
    if (read(fd, &res.type, sizeof(int)) < sizeof(int))
    {
        return 1;
    }
    if (read(fd, &res.nanosec, sizeof(int)) < sizeof(int))
    {
        return 1;
    }
    if (read(fd, &res.sec, sizeof(long)) < sizeof(long))
    {
        return 1;
    }
    return 0;
}

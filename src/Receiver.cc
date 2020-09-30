#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <chrono>
#include <thread>

#include "Log.hh"
#include "Util.hh"

static char usage[] = 
    "Usage: %s [OPTIONS] \n"
    "  -b [IP]\n"
    "    Set the source address to [IP].\n"
    "    Default: Let the system to determine.\n"
    "  -c [IP] (REQUIRED)\n"
    "    Connect to [IP].\n"
    "  -h\n"
    "    Display this message and quit.\n"
    "  -p [port] (REQUIRED)\n"
    "    Connect to [port].\n"
    "  -v\n"
    "    Display version information.\n"
    "  -w [path]\n"
    "    Print compact performance log to [path]. Print to stdout if [path]"
    "    is \"-\".\n";

static sockaddr_in addr = {0};
static sockaddr_in svaddr = {0};
static CompactRecorder rec;

static int parseArguments(int argc, char **argv)
{
    char c;
    int ret;
    
    while ((c = getopt(argc, argv, "b:c:hp:vw:")) != EOF)
    {
        switch (c)
        {
        case 'b':
            if (inet_aton(optarg, &addr.sin_addr) == 0)
            {
                log.error("parseArguments: Invalid IP string %s", optarg);
                return 1;
            }
            break;
        case 'c':
            if (inet_aton(optarg, &svaddr.sin_addr) == 0)
            {
                log.error("parseArguments: Invalid IP string %s", optarg);
                return 1;
            }
            break;
        case 'h':
            log.message(usage, argv[0]);
            return -1;
            break;
        case 'p':
            svaddr.sin_port = atoi(optarg);
            break;
        case 'v':
            log.message("Version %s\n", VERSION);
            return -1;
            break;
        case 'w':
            if ((ret = rec.init(optarg)) != 0)
            {
                log.error("parseArguments: Cannot open record file.");
                return -1;
            }
            break;
        default:
            log.error("parseArguments: Unrecognized option %c", c);
            return 2;
            break;
        }
    }

    if (svaddr.sin_addr.s_addr == 0)
    {
        log.error("parseArguments: Server address not specified.");
        return 3;
    }
    if (svaddr.sin_port == 0)
    {
        log.error("parseArguments: Server port not specified.");
        return 3;
    }

    return 0;
}

static long recvBuf[65536 / sizeof(long)], sendBuf[65536 / sizeof(long)];
static Message *rmsg = (Message*)recvBuf, *smsg = (Message*)sendBuf;
static int silent;
static int toAbort;
static int started;
static RWLock queueLock;
static std::list<long> recvQueue;

void sendMain(int fd)
{
    long sent = 0;
    char errbuf[64];
    socklen_t len = sizeof(svaddr);

    smsg->type = MessageType::INSTRUCTION;
    smsg->value = Instructions::START;
    while (!started && !toAbort)
    {
        if (sendto(fd, sendBuf, sizeof(Message), 0, 
            (struct sockaddr*)&svaddr, len) == -1)
        {
            log.error("sendMain: Socket broken when sending(%s).", 
                Log::strerror(errbuf));
            toAbort = 1;
            return;
        }
        log.message("sendMain: Start instruction sent.");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto st = std::chrono::system_clock::now();
    while (!toAbort)
    {
        long seq;
        queueLock.writeLock();
        if (recvQueue.empty())
        {
            queueLock.writeRelease();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        else
        {
            seq = recvQueue.front();
            recvQueue.pop_front();
            queueLock.writeRelease();
            smsg->type = MessageType::ACK;
            smsg->value = seq;
            len = sizeof(svaddr);
            if (sendto(fd, sendBuf, sizeof(Message), 0, 
                (struct sockaddr*)&svaddr, len) == -1)
            {
                log.error("sendMain: Socket broken when sending(%s).", 
                    Log::strerror(errbuf));
                toAbort = 1;
                return;
            }
            log.verbose("sendMain: ACK of packet %ld sent.", seq);
            ++sent;
            rec.write(seq, CompactRecorder::Type::ACK_SENT);
        }

        auto current = std::chrono::system_clock::now();
        if (current - st > std::chrono::milliseconds(3000))
        {
            smsg->type = MessageType::INSTRUCTION;
            smsg->value = Instructions::START;
            for (int i = 0; i < 5; ++i)
            {
                if (sendto(fd, sendBuf, sizeof(Message), 0, 
                    (struct sockaddr*)&svaddr, len) == -1)
                {
                    log.error("sendMain: Socket broken when sending(%s).", 
                        Log::strerror(errbuf));
                    toAbort = 1;
                    return;
                }
            }
            log.message("sendMain: Periodic start instruction sent.");
            st = current;
        }
    }

    log.message("sendMain: %ld ACKs sent.", sent);
}

void recvMain(int fd)
{
    long received = 0;
    int size;
    sockaddr_in recvInfo;
    char errbuf[64];

    while (!toAbort)
    {
        socklen_t len = sizeof(recvInfo);
        if ((size = recvfrom(fd, recvBuf, 65536, 0, 
            (struct sockaddr*)&recvInfo, &len)) == -1)
        {
            log.error("recvMain: Socket broken when receiving(%s).",
                Log::strerror(errbuf));
            toAbort = 1;
            return;
        }

        switch (rmsg->type)
        {
        case MessageType::DATA:
            if (svaddr.sin_addr.s_addr != recvInfo.sin_addr.s_addr ||
                svaddr.sin_port != recvInfo.sin_port)
            {
                log.warning("recvMain: Packet %ld from unknown Sender %s:%d.", 
                    rmsg->value, inet_ntoa(recvInfo.sin_addr), 
                    recvInfo.sin_port);
            }
            else
            {
                log.verbose("recvMain: Packet %ld received.", rmsg->value);
                if (received++ == 0)
                {
                    log.verbose("recvMain: First packet received.");
                }
                rec.write(rmsg->value, CompactRecorder::Type::RECEIVED);
                queueLock.writeLock();
                recvQueue.push_back(rmsg->value);
                queueLock.writeRelease();
                started = 1;
            }
            break;
        default:
            // ignore
            break;
        }
    }

    log.message("recvMain: %ld packets received.", received);
}

void sigHandler(int sig, siginfo_t *info, void *ptr)
{
    log.message("sigHandler: Signal %d received", sig);
    toAbort = 1;
}

int main(int argc, char **argv)
{
    int fd;
    int ret;
    char errbuf[64];

    signalNoRestart(SIGINT, sigHandler);
    log.message("This is UDPNetProbe Receiver, Version %s", VERSION);

    ret = parseArguments(argc, argv);
    if (ret < 0)
    {
        return 0;
    }
    else if (ret > 0)
    {
        log.error("main: Not recoverable, exit.");
        return 1;
    }

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        log.error("main: Cannot create socket(%s).", Log::strerror(errbuf));
        return 2;
    }

    if ((ret = bind(fd, (sockaddr*)&addr, sizeof(addr))) < 0)
    {
        log.error("main: Cannot bind to specified address %s(%s).", 
            inet_ntoa(addr.sin_addr), Log::strerror(errbuf));
        return 3;
    }

    std::thread sender(sendMain, fd), receiver(recvMain, fd);

    sender.join();
    receiver.join();

    return 0;
}

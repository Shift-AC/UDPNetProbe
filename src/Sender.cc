#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <chrono>
#include <thread>

#include "Log.hh"
#include "Util.hh"

static char usage[] = 
    "Usage: %s [OPTIONS] \n"
    "  -b [IP]\n"
    "    Set the source address to [IP].\n"
    "    Default: Let the system to determine.\n"
    "  -h\n"
    "    Display this message and quit.\n"
    "  -i [interval]\n"
    "    Set the [interval] in microseconds between two data packets.\n"
    "    Default: 100\n"
    "  -l [port] (REQUIRED)\n"
    "    Listen on [port].\n"
    "  -s [size]\n"
    "    Set the size of data packets.\n"
    "    Default: 1400\n"
    "  -v\n"
    "    Display version information.\n"
    "  -w [path] (default none(no output))\n"
    "    Print compact performance log to [path]. Print to stdout if [path]\n"
    "    is \"-\".\n";

static sockaddr_in addr = {0};
static CompactRecorder rec;
static int pakSize = 1400;
static int interval = 100;

static int parseArguments(int argc, char **argv)
{
    char c;
    int ret;
    
    while ((c = getopt(argc, argv, "b:hi:l:s:vw:")) != EOF)
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
        case 'h':
            log.message(usage, argv[0]);
            return -1;
            break;
        case 'i':
            interval = atoi(optarg);
            break;
        case 'l':
            addr.sin_port = htons(atoi(optarg));
            break;
        case 's':
            pakSize = atoi(optarg);
            break;
        case 'v':
            log.message("Version: %s\n", VERSION);
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
            log.error("parseArguments: Unrecognized option %c(%d)", c, c);
            return 2;
            break;
        }
    }
    if (addr.sin_port == 0)
    {
        log.error("parseArguments: Port to listen on not specified.");
        return 3;
    }

    return 0;
}

static long recvBuf[65536 / sizeof(long)], sendBuf[65536 / sizeof(long)];
static Message *rmsg = (Message*)recvBuf, *smsg = (Message*)sendBuf;
static int silent;
static sockaddr_in currentClient;
static int toAbort;

void sendMain(int fd)
{
    long sent = 0;
    char errbuf[64];
    int init = 1;
    auto st = std::chrono::system_clock::now();

    smsg->type = MessageType::DATA;
    smsg->value = 0;
    while (!toAbort)
    {
        socklen_t len = sizeof(currentClient);
        if (currentClient.sin_addr.s_addr == 0|| sent >= 1000000)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        else if (init)
        {
            st = std::chrono::system_clock::now();
            init = 0;
        }
        
		if (sendto(fd, sendBuf, pakSize, 0, 
			(struct sockaddr*)&currentClient, len) == -1)
		{
			log.error("sendMain: Socket broken when sending(%s).", 
                Log::strerror(errbuf));
			toAbort = 1;
			return;
		}

        log.verbose("sendMain: Packet %ld sent.", smsg->value);
		++sent;
            if (sent == 1000000)
            {
                log.message("SENT");
            }
        rec.write(smsg->value++, CompactRecorder::Type::SENT);
        std::this_thread::sleep_until(
            st += std::chrono::microseconds(interval));
	}
    
    log.message("sendMain: %ld packets sent.", sent);
}

void recvMain(int fd)
{
    long acked = 0;
    int size;
    sockaddr_in clientInfo;
    char errbuf[64];

    while (!toAbort)
    {
        socklen_t len = sizeof(currentClient);
        if ((size = recvfrom(fd, recvBuf, 65536, MSG_DONTWAIT, 
            (struct sockaddr*)&clientInfo, &len)) == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            else
            {
                log.error("recvMain: Socket broken when receiving(%s).",
                    Log::strerror(errbuf));
                toAbort = 1;
                return;
            }
        }

        switch (rmsg->type)
        {
        case MessageType::INSTRUCTION:
            if (rmsg->value == Instructions::START)
            {
                log.message("recvMain: Received start instruction from %s:%d.", 
                    inet_ntoa(clientInfo.sin_addr), ntohs(clientInfo.sin_port));
                currentClient = clientInfo;
            }
            break;
        case MessageType::ACK:
            if (currentClient.sin_addr.s_addr != clientInfo.sin_addr.s_addr ||
                currentClient.sin_port != clientInfo.sin_port)
            {
                log.warning("recvMain: ACK of packet %ld from unknown receiver"
                    "%s:%d.", rmsg->value, inet_ntoa(clientInfo.sin_addr), 
                    ntohs(clientInfo.sin_port));
            }
            else
            {
                log.verbose("recvMain: ACK of packet %ld received.", 
                    rmsg->value);
                ++acked;
                rec.write(rmsg->value, CompactRecorder::Type::ACKED);
            }
            break;
        default:
            // ignore
            break;
        }
    }

    log.message("recvMain: %ld packets ACKed.", acked);
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
    log.message("This is UDPNetProbe Sender, Version %s", VERSION);

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

    addr.sin_family = AF_INET;
    if ((ret = bind(fd, (sockaddr*)&addr, sizeof(addr))) < 0)
    {
        log.error("main: Cannot bind to specified address %s:%d(%s).", 
            inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), 
			Log::strerror(errbuf));
        return 3;
    }

	log.message("main: Listening...");

    std::thread sender(sendMain, fd), receiver(recvMain, fd);

    sender.join();
    receiver.join();

    return toAbort * 4;
}

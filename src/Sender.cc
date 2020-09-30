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
    "  -l [port] (REQUIRED)\n"
    "    Listen on [port].\n"
    "  -v\n"
    "    Display version information.\n"
    "  -w [path]\n"
    "    Print compact performance log to [path]. Print to stdout if [path]"
    "    is \"-\".\n";

static sockaddr_in addr = {0};
static CompactRecorder rec;

static int parseArguments(int argc, char **argv)
{
    char c;
    int ret;
    
    while ((c = getopt(argc, argv, "b:hl:vw:")) != EOF)
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
        case 'l':
            addr.sin_port = atoi(optarg);
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
            log.error("parseArguments: Unrecognized option %c", c);
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
    char errbuf[64];
    auto st = std::chrono::system_clock::now();

    smsg->type = MessageType::DATA;
    smsg->value = 0;
    while (!toAbort)
    {
        socklen_t len = sizeof(currentClient);
        if (currentClient.sin_addr.s_addr == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        else
        {
            st = std::chrono::system_clock::now();
        }
        
		if (sendto(fd, sendBuf, PAK_SIZE, 0, 
			(struct sockaddr*)&currentClient, len) == -1)
		{
			log.error("sendMain: Socket broken when sending(%s).", 
                Log::strerror(errbuf));
			toAbort = 1;
			return;
		}

        log.verbose("sendMain: Packet %ld sent.", smsg->value++);
        std::this_thread::sleep_until(st += std::chrono::microseconds(100));
    }
}

void recvMain(int fd)
{
    int size;
    sockaddr_in clientInfo;
    char errbuf[64];

    while (true)
    {
        socklen_t len = sizeof(currentClient);
        if ((size = recvfrom(fd, recvBuf, 65536, 0, 
            (struct sockaddr*)&clientInfo, &len)) == -1)
        {
            log.error("recvMain: Socket broken when receiving(%s).",
                Log::strerror(errbuf));
            toAbort = 1;
            return;
        }

        switch (rmsg->type)
        {
        case MessageType::INSTRUCTION:
            if (rmsg->value == Instructions::START)
            {
                log.message("recvMain: Received start instruction from %s:%d.", 
                    inet_ntoa(clientInfo.sin_addr), clientInfo.sin_port);
                currentClient = clientInfo;
            }
            break;
        case MessageType::ACK:
            if (currentClient.sin_addr.s_addr != clientInfo.sin_addr.s_addr ||
                currentClient.sin_port != clientInfo.sin_port)
            {
                log.warning("recvMain: ACK of packet %ld from unknown receiver"
                    "%s:%d.", rmsg->value, inet_ntoa(clientInfo.sin_addr), 
                    clientInfo.sin_port);
            }
            else
            {
                log.verbose("recvMain: ACK of packet %ld received.", 
                    rmsg->value);
            }
            break;
        default:
            // ignore
            break;
        }
    }

}

int main(int argc, char **argv)
{
    int fd;
    int ret;
    char errbuf[64];

	log.message("1");

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

	log.message("2");

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

	log.message("main: Listening...");

    std::thread sender(sendMain, fd), receiver(recvMain, fd);

    sender.join();
    receiver.join();

    return toAbort * 4;
}

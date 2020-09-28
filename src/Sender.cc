#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

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
    "    Display version information.\n";

static sockaddr_in addr = {0};

static int parseArguments(int argc, char **argv)
{
    char c;
    int ret;
    
    while ((c = getopt(argc, argv, "b:hl:v")) != EOF)
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
            printf(usage, argv[0]);
            return -1;
            break;
        case 'l':
            addr.sin_port = atoi(optarg);
            break;
        case 'v':
            printf("%s\n", VERSION);
            return -1;
            break;
        default:
            log.error("parseArguments: Unrecognized option %c", c);
            return 2;
            break;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    int ret;
    char errbuf[64];
    sockaddr_in clientInfo;
    socklen_t len = sizeof(clientInfo);
    long recvBuf[65536];
    int size;
    Message *msg = (Message*)recvBuf;
    
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

    if ((size = recvfrom(fd, recvBuf, 65536, 0, 
        (struct sockaddr*)&clientInfo, &len)) == -1)
    {
        log.error("main: Socket broken when receiving(%s).",
            Log::strerror(errbuf));
        return 4;;
    }

    log.message("main: Received start instruction from %s:%d.", 
        inet_ntoa(clientInfo.sin_addr), clientInfo.sin_port);



    while (true)
    {
		if (sendto(connfd, recvBuf, size, 0, 
			(struct sockaddr *)&clientInfo, len) == -1)
		{
			logError("Socket broken when sending(%s), trying to restart...",
				strerrorV(errno, errbuf));
			reinit = 1;
			continue;
		}

    }

    return 0;
}
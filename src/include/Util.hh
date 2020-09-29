#ifndef __UTIL_HH__
#define __UTIL_HH__

const char VERSION[] = "0.1.0.200928";

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

#endif

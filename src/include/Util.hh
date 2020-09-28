#ifndef __UTIL_HH__
#define __UTIL_HH__

const char VERSION[] = "0.1.0.200928";

enum MessageType
{
    RAW,
    DATA,
    ACK,
    INSTRUCTION
};

struct Message
{
    MessageType type;
    int value;
};

#endif

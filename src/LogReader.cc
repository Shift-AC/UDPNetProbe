#include "Util.hh"

#include <map>

std::map<int, const char*> typeText;

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: %s [file]", argv[0]);
        return 0;
    }

    typeText.insert({CompactRecorder::Type::SENT, "Sent"});
    typeText.insert({CompactRecorder::Type::RECEIVED, "Received"});
    typeText.insert({CompactRecorder::Type::ACK_SENT, "ACK Sent"});
    typeText.insert({CompactRecorder::Type::ACKED, "ACKed"});
    typeText.insert({CompactRecorder::Type::IGNORED, "Ignored"});

    RecordReader rd;
    CompactRecorder::Record rec;

    printf("%12s%12s    %s\n", "Seq", "Msg Type", "Timestamp");
    rd.init(argv[1]);
    while (rd.next(rec) == 0)
    {
        printf("%12ld%12s    %ld.%09d\n", rec.pakSeq, typeText[rec.type],
            rec.sec, rec.nanosec);
    }

    return 0;
}

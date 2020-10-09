#!/bin/bash

for ((i = 0; i < 100; ++i))
do
    if [ $((i & 1)) = 0 ]; then
        n=1000000000
    else
        n=1
    fi

    bin/udpnetprobe-receiver -c 192.168.0.144 -p 23333 -n $n 2>&1 | tee temp/$i.log &
    sleep 3600
    kill -2 $(pidof udpnetprobe-receiver)
done


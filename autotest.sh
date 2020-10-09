#!/bin/bash

while [[ true ]]
do
    fn=temp/`date +%y%m%d-%H%M%S`.log
    sudo bin/udpnetprobe-sender -l 23333 -i 20 2>&1 | tee $fn &
    pid=$!

    while ! grep -v -q SENT $fn
    do
        sleep 1
    done
    st=`awk '/recvMain/{print $4}' $fn | head -n 1 | cut -f 1 -d ")"`
    ed=`awk '/SENT/{print $4}' $fn | head -n 1 | cut -f 1 -d ")"`
    echo $ed-$st | bc
    
    sleep 10
done
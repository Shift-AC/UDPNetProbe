#!/bin/bash

while [[ true ]]
do
    fn=temp/`date +%y%m%d-%H%M%S`.log
    #echo Log file: $fn
    sudo bin/udpnetprobe-sender -l 23333 -i 20 2>&1 | tee $fn >/dev/null &

    x=0
    while ! grep -q SENT $fn
    do
        sleep 1
	x=$((x+1))
        if [ $x = 60 ]; then
            break;
        fi
    done

    st=`awk -F '(' '/recvMain/{print $2}' $fn | head -n 1 | cut -f 1 -d ")"`
    ed=`awk -F '(' '/SENT/{print $2}' $fn | head -n 1 | cut -f 1 -d ")"`
    echo $ed-$st | bc

    sudo kill -2 `sudo pidof udpnetprobe-sender`
    sleep 10
done

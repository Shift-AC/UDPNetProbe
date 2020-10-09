#!/bin/bash

for ((i = 1; i <= $#; ++i))
do
    eval fn=\${${i}}
    st=`awk -F '(' '/recvMain/{print $2}' $fn | head -n 1 | cut -f 1 -d ")"`
    ed=`awk -F '(' '/SENT/{print $2}' $fn | head -n 1 | cut -f 1 -d ")"`
    echo $ed-$st | bc
done

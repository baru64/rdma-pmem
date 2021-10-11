#!/bin/bash

SERVER_IP=10.10.0.123

for MSIZE in 256 512 1024 2048 4096 8192 12288 16384 20480 24576 32768
do
    for THREADS in 1 2 4 8
    do
        iperf3 -c $SERVER_IP -l $MSIZE -P $THREADS -t 20 -i 20 -J --logfile ./iperf3-$MSIZE-$THREADS-$1.json 
    done
done

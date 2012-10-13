#!/bin/bash

DEVICE="";
if [[ $(uname) == "Darwin" ]]; then
        DEVICE="/dev/cu.usbmodem00000001";
else
        DEVICE="/dev/ttyACM0";
fi

TXT="";
TXT="-t";

if [[ "$1" == "play" ]] || [[ "$1" == "p" ]]; then
        # autoplay 4000ms
        sudo ./IRToy -d ${DEVICE} -v -p ${TXT} -a 4000 -f $2
elif [[ "$1" == "record" ]] || [[ "$1" == "r" ]] || [[ "$1" == "rec" ]]; then
        sudo ./IRToy -d ${DEVICE} -v -r ${TXT} -f $2
elif [[ "$1" == "reset" ]] || [[ "$1" == "x" ]]; then
        sudo ./IRToy -d ${DEVICE} -v -x
else
        echo "Not a valid command. Pick from: play, record";
fi

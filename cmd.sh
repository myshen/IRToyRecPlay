#!/bin/bash

DEVICE="";
if [[ $(uname) == "Darwin" ]]; then
        DEVICE="/dev/cu.usbmodem00000001";
else
        DEVICE="/dev/ttyACM0";
fi

CMD="./IRToy";

TXT="-t";
TXT="";

if [[ "$1" == "play" ]] || [[ "$1" == "p" ]]; then
        # autoplay 1000ms
        ${CMD} -d ${DEVICE} -v -p ${TXT} -a 1000 -f $2
elif [[ "$1" == "record" ]] || [[ "$1" == "r" ]] || [[ "$1" == "rec" ]]; then
        ${CMD} -d ${DEVICE} -v -r ${TXT} -f $2
elif [[ "$1" == "reset" ]] || [[ "$1" == "x" ]]; then
        ${CMD} -d ${DEVICE} -v -x
elif [[ "$1" == "interactive" ]] || [[ "$1" == "i" ]] || [[ "$1" == "int" ]]; then
        ${CMD} -d ${DEVICE} -v -i
else
        echo "Not a valid command. Pick from: play, record, interactive";
fi

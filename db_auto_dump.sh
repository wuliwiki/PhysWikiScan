#!/bin/bash

# usage:
#     sudo nohup ./sync.sh >> sync.log &

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

while true; do
    date
    echo -----------------------------------
    ./db_dump.sh
    printf "sleeping...\n\n\n"
    sleep 8h
done

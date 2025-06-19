#!/bin/bash
# Usage: ./talkA.sh <B_IP>

rec  -q -t raw -b 16 -c 1 -e s -r 44100 - | ./serv_send2 50000 &
./client_recv "$1" 60000 | play -q -t raw -b 16 -c 1 -e s -r 44100 -

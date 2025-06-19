#!/bin/bash
# Usage: ./talkB.sh <A_IP>

rec  -q -t raw -b 16 -c 1 -e s -r 44100 - | ./serv_send2 60000 &
./client_recv "$1" 50000 | play -q -t raw -b 16 -c 1 -e s -r 44100 -

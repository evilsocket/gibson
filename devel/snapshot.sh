#!/bin/bash

if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

GIBSON=$(pidof gibson)

if [ -z $GIBSON ]; then
    echo "Seems like gibson is not running" 1>&2
    exit 1
fi

echo "## PID: $GIBSON"
echo "## STACK:"
gdb -ex "set pagination 0" -ex "thread apply all bt" --batch -p $GIBSON

echo ""
echo "## NETWORK:"
netstat -nap | grep gibson

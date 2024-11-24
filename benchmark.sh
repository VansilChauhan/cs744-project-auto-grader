#!/bin/bash

# Variables
SERVER_IP=$1
SERVER_PORT=$2
PROGRAM_FILE=$3
CLIENT="./client.o"

# Check if required parameters are provided
if [[ -z "$SERVER_IP" || -z "$SERVER_PORT" || -z "$PROGRAM_FILE" ]]; then
    echo "Usage: $0 <server_ip> <server_port> <program_file>"
    exit 1
fi

# Run client.o with parallel execution using xargs
# for i in {1..40}; do echo $i; done |
xargs -n 1 -P 10 bash -c "${CLIENT} $SERVER_IP $SERVER_PORT $PROGRAM_FILE"

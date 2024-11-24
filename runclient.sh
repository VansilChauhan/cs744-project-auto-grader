#!/bin/bash

MAX=100
SERVER_IP="127.0.0.1"
SERVER_PORT="8080"
CLIENT="./client.o"

# Generate tasks and feed them to xargs
for ((i = 1; i <= $MAX; i++)); do
    CURR=$(($i % 4 + 1)) # This determines the task (1, 2, 3, or 4)
    echo $CURR
done | xargs -P 10 -I {} bash -c "${CLIENT} $SERVER_IP $SERVER_PORT program_{}.c"

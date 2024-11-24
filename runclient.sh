#!/bin/bash
MAX=100
for ((i = 2; i <= $MAX; i++)); do
    ./client.o 127.0.0.1 8080 $1
done

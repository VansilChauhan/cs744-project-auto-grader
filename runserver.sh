#!/bin/bash
make clean
make
./server.o 8080 1 solution.c test_cases.txt

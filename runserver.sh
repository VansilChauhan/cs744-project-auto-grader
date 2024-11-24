#!/bin/bash
make clean
make
./server.o 8080 100 solution.c test_cases.txt

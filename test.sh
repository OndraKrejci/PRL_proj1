#!/bin/bash

numbers=8;
procs=19;

mpic++ --prefix /usr/local/share/OpenMPI -o oems oems.cpp

# vyrobeni souboru s random cisly
dd if=/dev/random bs=1 count=$numbers of=numbers status=none

mpirun --prefix /usr/local/share/OpenMPI --oversubscribe -np $procs oems

rm -f oems numbers

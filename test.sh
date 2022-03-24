#!/bin/bash

numbers=8;

mpic++ --prefix /usr/local/share/OpenMPI -o oems oems.cpp

# vyrobeni souboru s random cisly
dd if=/dev/random bs=1 count=$numbers of=numbers

mpirun --prefix /usr/local/share/OpenMPI -np $numbers oems

rm -f oems numbers

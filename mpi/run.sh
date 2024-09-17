#!/bin/bash

export LD_LIBRARY_PATH=..:$LD_LIBRARY_PATH

export USE_ELFUTILS=1

$HOME/Apps/openmpi/latest/bin/mpirun -n 1 ./trace.sh ./app

#!/bin/bash

export LD_LIBRARY_PATH=/home/gllort/Work/libaddr2line.git/install/lib:$LD_LIBRARY_PATH

export LIBADDR2LINE_BACKEND=binutils

$HOME/Apps/openmpi/latest/bin/mpirun -n 1 ./trace.sh ./app

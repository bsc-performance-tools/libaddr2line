#!/bin/bash

export LD_LIBRARY_PATH=..:$LD_LIBRARY_PATH

mpirun -n 1 ./trace.sh ./app

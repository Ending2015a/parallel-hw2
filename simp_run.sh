#!/bin/bash

low="-0.3"
up="0"
left="0.7"
right="1"
width="1280"
height="720"

#trap '[[ $BASH_COMMAND != echo* ]] && echo $BASH_COMMAND' DEBUG

set -x

time srun -p batch -N 4 -n 8 ./ms_seq 1 $low $up $left $right $width $height seq.png
time srun -p batch -N 4 -n 8 ./ms_mpi_static 1 $low $up $left $right $width $height mpi.png
time srun -p batch -N 4 -n 8 ./ms_mpi_dynamic 1 $low $up $left $right $width $height dyn.png

hw2-diff seq.png mpi.png
hw2-diff seq.png dyn.png

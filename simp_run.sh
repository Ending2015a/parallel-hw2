#!/bin/bash

low="-2"
up="2"
left="-2.25"
right="1.75"
width="1280"
height="720"

#trap '[[ $BASH_COMMAND != echo* ]] && echo $BASH_COMMAND' DEBUG

set -x

time srun -p batch -N 4 -n 8 ./ms_seq 1 $low $up $left $right $width $height seq.png
time srun -p batch -N 4 -n 8 ./ms_mpi_static 1 $low $up $left $right $width $height mpi.png
time srun -p batch -N 4 -n 8 ./ms_mpi_dynamic 1 $low $up $left $right $width $height dyn.png
time srun -p batch -N 1 -n 1 -c 8 ./ms_omp 8 $low $up $left $right $width $height omp.png


hw2-diff seq.png mpi.png
hw2-diff seq.png dyn.png
hw2-diff seq.png omp.png

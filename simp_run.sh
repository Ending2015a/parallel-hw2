#!/bin/bash

low="-1"
up="1"
left="-1"
right="1"
width="1280"
height="720"

#trap '[[ $BASH_COMMAND != echo* ]] && echo $BASH_COMMAND' DEBUG

set -x

time srun -p batch -N 4 -n 8 ./ms_seq 1 $left $right $low $up $width $height seq.png
time srun -p batch -N 4 -n 8 ./ms_mpi_static 1 $left $right $low $up $width $height mpi.png
time srun -p batch -N 4 -n 8 ./ms_mpi_dynamic 1 $left $right $low $up $width $height dyn.png
time srun -p batch -N 1 -n 1 -c 8 ./ms_omp 8 $left $right $low $up $width $height omp.png
time srun -p batch -N 2 -n 4 -c 4 ./ms_hybrid 4 $left $right $low $up $width $height hyb.png

hw2-diff seq.png mpi.png
hw2-diff seq.png dyn.png
hw2-diff seq.png omp.png
hw2-diff seq.png hyb.png

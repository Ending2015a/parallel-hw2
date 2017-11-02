#!/bin/bash

ans_program='ms_seq'
ans_file='seq.png'
program='ms_mpi_dynamic'
output_dir='./measure_time/'
outfile="${output_dir}${program}_time.txt"
pic_name='dyn.png'
p='-p batch'
N_arr=('1' '1' '2' '4' '4' '4' '4' '4')
n_arr=('2' '4' '8' '16' '24' '32' '40' '48')
c_arr=('1' '1' '1' '1' '1' '1' '1' '1')
height=720
width=1280
low=-1
up=1
left=-1
right=1


pass=0
all_pass=1

echo "starting program $program"

if [ ! -f "$ans_file" ] ; then
    echo "generating ans file..."
    ./${ans_program} 1 $left $right $low $up $width $height $ans_file
    echo "ans file [ done ]"   
fi


if [ -f "$outfile" ] ; then
    echo "removing old $outfile..."
    rm $outfile
fi


for ((i=0;i<${#N_arr[@]};++i)); do
    echo ""
    N=${N_arr[i]}
    n=${n_arr[i]}
    c=${c_arr[i]}

    echo "N=$N n=$n c=$c left=$left right=$right low=$low up=$up width=$width height=$height" &>> $outfile
    echo "[ for case $i ] : -N=$N -n=$n -c=$c"
    

    echo "srun $p -N $N -n $n -c $c ./${program[$j]} ${c} $left $right $low $up $width $height $pic_name"
    { time srun $p -N $N -n $n -c $c ./${program[$j]} ${c} $left $right $low $up $width $height $pic_name &>> $outfile ; } &>> $ourfile

    echo "case done -> verifying..."

    result=$(hw2-diff $ans_file $pic_name)
    if echo "$result" | grep "100.00%" ; then
        echo "case $i: [  pass  ]"
        let pass="$pass + 1"
    else
        echo "case $1: [ failed ]" >> outfile 2>&1
        let all_pass=0
    fi
done

echo "pass $pass/${#N_arr[@]}"
if [ "${all_pass}" == "1" ] ; then
    echo "[ ALL PASS ] "
fi

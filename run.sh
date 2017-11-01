#!/bin/bash


program=('ms_seq' 'ms_mpi_static' 'ms_mpi_dynamic' 'ms_omp' 'ms_hybrid')
pic_name=('seq.png' 'stc.png' 'dyn.png' 'omp.png' 'hyb.png')
total_case_gen=10
p='-p batch'
N=('1' '4' '4' '1' '4')
n=('1' '8' '8' '1' '8')
c=('1' '1' '1' '8' '4')
height=1280
width=720
program_num=${#program[@]}
pass=0
all_pass=1
lower=-2
upper=2



for ((i=0;i<$total_case_gen;++i)); do
    
    low=$(python gen_rand.py $lower $upper)
    up=$(python gen_rand.py $low $upper)
    left=$(python gen_rand.py $lower $upper)
    right=$(python gen_rand.py $left $upper)

    echo "for case$i: left=$left right=$right low=$low up=$up"

    for ((j=0;j<$program_num;++j)); do
        echo "srun $p -N ${N[$j]} -n ${n[$j]} -c ${c[$j]} ./${program[$j]} ${c[$j]} $left $right $low $up $width $height ${pic_name[$j]}"
        time srun $p -N ${N[$j]} -n ${n[$j]} -c ${c[$j]} ./${program[$j]} ${c[$j]} $left $right $low $up $width $height ${pic_name[$j]}
    done

    echo "case done -> verifying..."

    case_pass=1
    for ((j=1;j<$program_num;++j)); do
        ans=${pic_name[0]}
        hw2-diff $ans ${pic_name[$j]} >$(tty)
        if hw2-diff $ans ${pic_name[$j]} | grep "100.00%" ; then
            echo -e "program ${program[$j]}: [ pass ]"
        else
            echo -e "program ${program[$j]}: [failed]"
            let all_pass=0
            let case_pass=0
        fi
    done
    if [ "${case_pass}" == "1" ] ; then
        let pass="$pass + 1"
        echo -e "case $i: [ pass ]"
    else
        echo -e "case $i: [failed]"
    fi
done

echo "pass $pass/$total_case_gen"
if [ "${all_pass}" == "1" ] ; then
    echo -e "[ ALL PASS] "
fi

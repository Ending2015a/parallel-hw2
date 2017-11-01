#!/bin/bash


program=('ms_seq' 'ms_mpi_static' 'ms_mpi_dynamic' 'ms_omp' 'ms_hybrid')
total_case_gen=10
p='-p batch'
N=('1' '4' '4' '1' '4')
n=('1' '8' '8' '1' '8')
c=('1' '1' '1' '8' '8')
program_num=${#program[@]}
pass=0
all_pass=1




for ((i=0;i<$total_case_gen;++i)); do
    
    echo "for case$i: -N=$N -n=$n"

    echo "N $N, n $n" >> $log
    echo "time srun $p -N $N -n $n $prog $number $cs $out"
    { time srun $p -N $N -n $n $prog $number $cs $out >> $log ; } 2>> $log

    echo "case done -> verifying..."


    if ./tools/check $ans $out ; then
        let pass="$pass + 1"
        echo -e "case$i: [ \033[1mpass\033[1m ]"
    else
        echo -e "case$i: [ \033[1mfailed\033[1m ]"
        let all_pass=0
    fi
done

echo "pass $pass/$total"
if [ "${all_pass}" == "1" ] ; then
    echo -e "\033[1mALL PASS\033[1m"
fi

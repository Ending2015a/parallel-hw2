#!/bin/bash


orig='./ms_seq'
prog='./HW1_103062372_advanced'
p='-p batch'
declare -a Narr=('1'  '4'  '4'  '4')
declare -a narr=('1' '4' '16' '48')
number=1
pf='1'
cs="testcase/case${pf}"
ans="testcase/case${pf}.ans"
out="out${pf}"
log="advanced_log${pf}_1.txt"

total=${#Narr[@]}
pass=0
all_pass=1

rm $log
rm $out

for ((i=0;i<$total;++i)); do
    N=${Narr[i]}
    n=${narr[i]}
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

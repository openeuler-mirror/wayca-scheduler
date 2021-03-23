#!/bin/bash

# set -x

# The benchmarking command to run
COMMAND=${CMD:-"/home/guodong/attractivechaos.benchmarks.git/lock/lock_test"}

REPETITION=3

# 3-pthread-spin; 4-mutex; 5-semaphore
for (( type=3; type<=5; type+=1 ))
do

for cpu_binding in "0-3" "0-8" "0-15" "0-23" "0,4,8,12" "0,6,12,18" "0,1,24,25" "0,1,48,49" "0,1,72,73" "0,24,48,72"
do

	for (( repetition=1; repetition<=$REPETITION; repetition+=1 ))
	do
		echo "Benching CPU $cpu_binding for lock_type $type, round $repetition:"
		numactl --physcpubind=$cpu_binding --membind=0 $COMMAND -t 4 -n 1000000 -m 100 -l $type  2>&1
	done
done
done


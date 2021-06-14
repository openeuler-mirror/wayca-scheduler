#!/bin/bash

DataFile=./wayca_sc_group_test.log

echo "" > $DataFile

group_num=(1 4 5 8)
thread_num=(1 4 32)
topo_level=(CPU CCL NUMA PACKAGE)

for groups in ${group_num[@]}
do
	export WAYCA_TEST_GROUPS=$groups

	for group_topo in ${topo_level[@]}
	do
		export WAYCA_TEST_GROUP_TOPO_LEVEL=$group_topo

		for threads in ${thread_num[@]}
		do
			export WAYCA_TEST_GROUP_ELEMS=$threads

			for thread_topo in ${topo_level[@]}
			do
				export WAYCA_TEST_THREAD_TOPO_LEVEL=$thread_topo

				echo "=====Group number: ${groups} Group Topo: ${group_topo}  Thread number: ${threads} Thread Topo: ${thread_topo}=====" >> $DataFile

				echo "Free Scatter" >> $DataFile
				./wayca_sc_group &>>$DataFile
				printf "result is %d \n" $? >>$DataFile

				export WAYCA_TEST_THREAD_BIND_PERCPU=1
				echo "PerCpu Scatter" >> $DataFile
				./wayca_sc_group &>>$DataFile
				printf "result is %d \n" $? >>$DataFile
				unset WAYCA_TEST_THREAD_BIND_PERCPU

				export WAYCA_TEST_THREAD_COMPACT=1
				echo "Free Compact" >> $DataFile
				./wayca_sc_group &>>$DataFile
				printf "result is %d \n" $? >>$DataFile
				unset WAYCA_TEST_THREAD_COMPACT

				export WAYCA_TEST_THREAD_BIND_PERCPU=1
				export WAYCA_TEST_THREAD_COMPACT=1
				echo "PerCpu Compact" >> $DataFile
				./wayca_sc_group &>>$DataFile
				printf "result is %d \n" $? >>$DataFile
				unset WAYCA_TEST_THREAD_BIND_PERCPU
				unset WAYCA_TEST_THREAD_COMPACT
			done
		done
	done
done

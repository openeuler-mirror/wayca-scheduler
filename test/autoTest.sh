#!/bin/bash

DataFile=./test.log

for i in $(seq 2 2 10)
do
	export WAYCA_TEST_GROUPS=$i
	export WAYCA_TEST_GROUP_ELEMS=$i

	file=$DataFile
	echo "${i} * ${i}, 10000*10000*1" >> $file

	./wayca_group &>>$file
done

for i in $(seq 3 2 11)
do
	export WAYCA_TEST_GROUPS=$i
	export WAYCA_TEST_GROUP_ELEMS=$i

	file=$DataFile
	echo "${i} * ${i}, 10000*10000*1" >> $file

	./wayca_group &>>$file
done
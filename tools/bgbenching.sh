#!/bin/bash

# The benchmarking command to run
COMMAND=${CMD:-"/home/guodong/lmbench-3.0-a9/bin/lat_syscall"}
PARAMETERS=${PARAM:-"-P 2 -N 10000 read"}

# Number of CPU cores per NUMA node
CORES_PER_NODE=${CORES_PER_NODE:-24}

for i in {0..3}
do
{
echo "Running on NUMA Node $i: $COMMAND $PARAMETERS ..."
# numactl --physcpubind=$((i*CORES_PER_NODE)) --membind=$i $COMMAND $PARAMETERS
numactl --physcpubind=$((i*CORES_PER_NODE)) --membind=0 $COMMAND $PARAMETERS
echo "Finished on NUMA Node $i"
} &
done

sleep 1
echo "Now waiting ..."
wait   ## wait all backgroud processes finish
echo "All finished"

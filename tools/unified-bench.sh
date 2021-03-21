#!/bin/bash

# set -x;
CPU_BINDINGS=("0-3" "0-7" "0-11" "0-23" "0,4,8,12");
CMD=/home/guodong/lmbench-3.0-a9/bin/lat_syscall;
for cat in null read write
  do
  for i in 1 2 3 4 8 12 24 48 72 96
    do
    PARAM="-P ${i} -N 10 -W 2 ${cat}"
    for cpu_binding in "${CPU_BINDINGS[@]}"
      do
      echo "${cat}:Jobs:${i}:CPUs:${cpu_binding}:Result:"
      numactl --physcpubind=$cpu_binding $CMD $PARAM
      done;
    done;
  done;


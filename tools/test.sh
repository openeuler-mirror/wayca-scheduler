#!/bin/bash

set -x
source base.sh

###############################################################################
# CONFIGURATION SECTION
###############################################################################

#  - IS_8CCL="Y": each NUMA node has 8 CCLs
#  - IS_8CCL="N": each NUMA node has 6 CCLs
IS_8CCL=${IS_8CCL:-"Y"}

#  - COMPACTED_2JOBS_PER_CCL_PLACEMENT="Y": always put 2 jobs into one CCL when possible
#  - COMPACTED_2JOBS_PER_CCL_PLACEMENT="N": place jobs as far between as possible
COMPACTED_2JOBS_PER_CCL_PLACEMENT="Y"
# benchmark tools and parameters: stream
STREAM_CMD=${STREAM:-"/usr/lib/lmbench/bin/stream"}
PARAM="-M 1024M -N 5"
EXTRA_PARAM=""
# benchmark tools and parameters: lat_pipe
LAT_PIPE_CMD=${LAT_PIPE:-"./lat_pipe"}
# benchmark tools for lock benchmark: lock_test
LOCK_CMD=${LOCK_TEST:-"./lock_test"}
# Usage: lock_test [-t nThreads=1] [-n size=1000000] [-m repeat=100] [-l lockType=1]
# Lock type: 0 for single-thread; 1 for gcc builtin; 2 for spin lock; 3 for pthread spin; 4 for mutex;
#            5 for semaphore; 6 for buffer+spin; 7 for buffer+mutex


if [ $IS_8CCL = "Y" ]; then
    echo "this is a 8CCL machine"
    N_CCL_PER_NODE=8
else
    echo "this is a 6CCL machine"
    N_CCL_PER_NODE=6
fi

# To check availability of tools:
# 1. whether ./spreads exist or not
if [ ! -f ./spreads ]
then
	make clean
	make all
fi
# 2. whether 'stream' exist or not
if [ ! -f $STREAM_CMD ]
then
	echo "FATAL: Benchmark tool $STREAM_CMD doesn't exist. Please fix."
	exit
fi
# 3. whether 'lat_pipe' exist or not
if [ ! -f $LAT_PIPE_CMD ]
then
	echo "Warning: Benchmark tool $LAT_PIPE_CMD doesn't exist."
fi
# 4. whether 'lock_test' exist or not
if [ ! -f $LOCK_CMD ]
then
	echo "Warning: Benchmark tool $LOCK_CMD doesn't exist."
fi

###############################################################################
# BENCHMARK RUNNING
###############################################################################

# initialize benchmark environment
init_env

if [ 1 -eq 1 ]; then
# Section 1: CCLs bandwidth
for n_jobs in 1 4 6 8 12 16 20 24 28 32
do
    if [ $n_jobs -lt $N_CCL_PER_NODE ]; then         # TODO: can be defined as a function
	((n_ccl_max=n_jobs))
    else
	((n_ccl_max=N_CCL_PER_NODE))
    fi

    n_ccl=1
    while [ $n_ccl -le $n_ccl_max ]
    do
	core_list=`./spreads -t 0 -j $n_jobs -c $n_ccl -o 0`
        case "$core_list" in
	    *ERROR* ) ;;
	    *) numactl --physcpubind=$core_list $STREAM_CMD -P $n_jobs $PARAM;;
	esac
        # if [ $core_list != "ERROR" ]; then
        #    numactl --physcpubind=$core_list $STREAM_CMD -P $n_jobs $PARAM
        # fi
	((n_ccl++))
    done
done

# Section 2: NUMAs bandwidth
for n_jobs in 4 6 8 12 16 20 24 28 32                  # TODO: automatic expand to number_of_cores_in_one_NUMA
do
    n_nodes=1
    while [ $n_nodes -le 4 ]                           # TODO: 4 is total number of NUMA nodes
    do
	if [ $COMPACTED_2JOBS_PER_CCL_PLACEMENT = "Y" ]; then
	    # to use at least two cores in each CCL, whenever possible
	    ((n_ccls_allowed=((n_jobs+n_nodes-1)/n_nodes+1)/2))
	    if [ $n_ccls_allowed -gt $N_CCL_PER_NODE ]; then
		((n_ccls_allowed=N_CCL_PER_NODE))
	    fi
	else
	    n_ccls_allowed=$N_CCL_PER_NODE
	fi
	core_list=`./spreads -t 1 -j $n_jobs -n $n_nodes -l $N_CCL_PER_NODE -c $n_ccls_allowed -o 0`
        case "$core_list" in
	    *ERROR* ) ;;
	    *) numactl --physcpubind=$core_list $STREAM_CMD -P $n_jobs $PARAM;;
	esac
	((n_nodes++))
    done
done

# Section 3: computing on NUMA0 ==> memory on Node 0, 1, 2, 3
for n_jobs in 1 4 6 8 12 16                          # TODO: a) add 1 job case; b) automatic expand to 70% of number of cores in one NUMA node
do
    if [ $n_jobs -lt $N_CCL_PER_NODE ]; then         # TODO: can be defined as a function
	((n_ccl_max=n_jobs))
    else
	((n_ccl_max=N_CCL_PER_NODE))
    fi

    n_ccl=1
    while [ $n_ccl -le $n_ccl_max ]
    do
	core_list=`./spreads -t 0 -j $n_jobs -c $n_ccl -o 0`
        case "$core_list" in
	    *ERROR* ) ;;
	    *)
		CORE_ARRAY=($core_list)
		doit_membind_looping_only "$STREAM_CMD" "-P $n_jobs" "$PARAM" "${CORE_ARRAY[@]}"
		;;
		# numactl --physcpubind=$core_list $STREAM_CMD -P $n_jobs $PARAM;;
	esac
	((n_ccl++))
    done
done

# Section 4: memory interleaving on Nodes (0,1), (0,1,2), or (0,1,2,3), and computing follows
((n_jobs=$N_CCL_PER_NODE*4))
for n_nodes in 1 2 3 4
do
    core_list=`./spreads -t 1 -j $n_jobs -n $n_nodes -l $N_CCL_PER_NODE -c $N_CCL_PER_NODE -o 0`
    ((interleave_to=n_nodes-1))
    numactl --physcpubind=$core_list --interleave=0-$interleave_to $STREAM_CMD -P $n_jobs $PARAM
done

# Section 4, extension 1:  memory interleaving on Nodes (0,1), (0,1,2), or (0,1,2,3), and computing stays on Node 0
#                          - jobs spreads over a typical number of CCLs, i.e. 2 jobs per CCL.
for n_jobs in 4 6 8 12 16 20 24 28 32                  # TODO: no. of max jobs up to number of cores in a NUMA node
do
    ((n_ccl_tmp=(n_jobs+1)/2))
    if [ $n_ccl_tmp -lt $N_CCL_PER_NODE ]; then
	((n_ccl=n_ccl_tmp))
    else
	((n_ccl=N_CCL_PER_NODE))
    fi
    core_list=`./spreads -t 0 -j $n_jobs -c $n_ccl -o 0`
    case "$core_list" in
	*ERROR* ) ;;
	*)
	    for n_nodes in 1 2 3 4
	    do
		((interleave_to=n_nodes-1))
		numactl --physcpubind=$core_list --interleave=0-$interleave_to $STREAM_CMD -P $n_jobs $PARAM
	    done
	    ;;
    esac
done

# Section 4, extension 2:  memory interleaving on Nodes (0,1), (0,1,2), or (0,1,2,3), and computing stays on Node 0
#                          - jobs spreads over CCLs, from intensive cases 4 jobs per CCL, to loose cases 1 job per CCL.
if [ 1 -eq 0 ]; then

for n_jobs in 4 6 8 12 16 20 24 28 32
do
    n_ccl=1
    while [ $n_ccl -le $N_CCL_PER_NODE ]                               # jobs spreads over CCLs
    do
	core_list=`./spreads -t 0 -j $n_jobs -c $n_ccl -o 0`
        case "$core_list" in
	    *ERROR* ) ;;
	    *)
		for n_nodes in 1 2 3 4                                 # interleaving across NUMA nodes
		do
		    ((interleave_to=n_nodes-1))
		    numactl --physcpubind=$core_list --interleave=0-$interleave_to $STREAM_CMD -P $n_jobs $PARAM
		done
		;;
	esac
	((n_ccl++))
    done
done

fi

# Section 5: Pipe latency
PARAM="-W 1 -N 20"

# 5.1 between self and each CCL in the same NUMA node
idx_ccl=0
while [ $idx_ccl -lt $N_CCL_PER_NODE ]
do
    ((second_core=idx_ccl*4))
    # measure pipe latency between core 1 with all CCLs of the same NUMA node
    LMBENCH_SCHED="CUSTOM_SPREAD 1 $second_core" ${LAT_PIPE_CMD} ${PARAM}
    ((idx_ccl++))
done

# 5.2 between self and each NUMA nodes
idx_node=0
while [ $idx_node -lt 4 ]                        # TODO: 4 is the number of NUMA nodes in the server, need to automate
do
    ((second_core=idx_node*N_CCL_PER_NODE*4))    # TODO: 4 is the number of cores in each CCL
    # measure pipe latency between core 1 with all NUMA nodes
    LMBENCH_SCHED="CUSTOM_SPREAD 1 $second_core" ${LAT_PIPE_CMD} ${PARAM}
    ((idx_node++))
done
fi

# Section 6: Lock bench
for n_jobs in 4 8                              # number of parallel processes
do
PARAM="-t $n_jobs -n 1000000 -m 100"

# type: 3-pthread-spin; 4-mutex
for (( type=3; type<=4; type+=1 ))
do
    # 6.1: same NUMA node, lock latency
    if [ $n_jobs -lt $N_CCL_PER_NODE ]; then
	((n_ccl_max=n_jobs))
    else
	((n_ccl_max=N_CCL_PER_NODE))
    fi

    n_ccl=1
    while [ $n_ccl -le $n_ccl_max ]                               # jobs spreads over CCLs, same NUMA node
    do
	core_list=`./spreads -t 0 -j $n_jobs -c $n_ccl -o 0`
        case "$core_list" in
	    *ERROR* ) ;;
	    *)
		echo "Benching CPU $core_list for lock_type $type"
		numactl --physcpubind=$core_list $LOCK_CMD $PARAM -l $type  2>&1
		;;
	esac
	((n_ccl++))
    done

    # 6.2: cross two NUMA nodes, lock latency
    ((n_ccl=(n_jobs+3)/4))                     # two jobs per CCL
    # between two NUMA nodes
    ((N_CROSS_CCLS=N_CCL_PER_NODE))
    for n_nodes in 2 3 4                       # TODO: 4 is the number of NUMA nodes in the system
    do
	core_list=`./spreads -t 1 -j $n_jobs -n 2 -l $N_CROSS_CCLS -c $n_ccl -o 0`            # -n 2, to reuse `spreads`, here pretends there's only two nodes, with $n_nodes*$N_CCL_PER_NODE cores in each node
        case "$core_list" in
	    *ERROR* ) ;;
	    *)
		echo "Benching CPU $core_list for lock_type $type"
		numactl --physcpubind=$core_list $LOCK_CMD $PARAM -l $type  2>&1
		;;
	esac
	((N_CROSS_CCLS+=N_CCL_PER_NODE))
    done

    # 6.3: cross four NUMA nodes, lock latency
    ((n_ccl=(n_jobs+7)/8))                     # two jobs per CCL
    # between four NUMA nodes
    ((n_nodes=4))
    core_list=`./spreads -t 1 -j $n_jobs -n $n_nodes -l $N_CCL_PER_NODE -c $n_ccl -o 0`
    echo "Benching CPU $core_list for lock_type $type"
    numactl --physcpubind=$core_list $LOCK_CMD $PARAM -l $type  2>&1
done
done

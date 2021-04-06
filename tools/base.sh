#!/bin/bash

# Initialize the benchmarking environment
#  - some of commands run here need superuser previledge
function init_env()
{
	# set cpufreq to performance mode to achieve best possible benchmarks
	sudo `readlink -f /proc/$$/exe` -c 'for filename in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance > $filename ; done'
}

function doit()
{
	cmd=$1
	extra_param=$2
	param=$3
	shift 3
	cpu_bindings=("$@")

	for cpu_binding in "${cpu_bindings[@]}"
       	do
	       numactl --physcpubind=$cpu_binding --membind=0 $cmd $extra_param $param
	done
}

function doit_no_membind()
{
	cmd=$1
	extra_param=$2
	param=$3
	shift 3
	cpu_bindings=("$@")

	for cpu_binding in "${cpu_bindings[@]}"
       	do
	       numactl --physcpubind=$cpu_binding $cmd $extra_param $param
	done
}

function doit_membind_looping_only()
{
	cmd=$1
	extra_param=$2
	param=$3
	shift 3
	cpu_bindings=("$@")

	for cpu_binding in "${cpu_bindings[@]}"
	do
		for mem_binding in {0..3}
		do
			numactl --physcpubind=$cpu_binding --membind=$mem_binding $cmd $extra_param $param
		done
	done
}

# based on the _only version, added interleaving
function doit_membind_looping()
{
	cmd=$1
	extra_param=$2
	param=$3
	shift 3
	cpu_bindings=("$@")
	MEM_INTERLEAVES=("0" "0,1" "0,1,2" "0,1,2,3")

	for cpu_binding in "${cpu_bindings[@]}"
	do
		for mem_binding in {0..3}
		do
			numactl --physcpubind=$cpu_binding --membind=$mem_binding $cmd $extra_param $param
		done
		for mem_interleaving in "${MEM_INTERLEAVES[@]}"
		do
			numactl --physcpubind=$cpu_binding --interleave=$mem_interleaving $cmd $extra_param $param
		done
	done
}

# 1 job, one core
CPU_BINDINGS_1=("0")

# cross CCLs
# 4 jobs, spread over 1CCL/ 2CCL /3CCL /4CCL:
CPU_BINDINGS_4=("0-3" "0-1,4-5" "0-1,4,8" "0,4,8,12")
# 6 jobs, spread over 2CCL /3CCL /4CCL/ 5CCL /6CCLs:
CPU_BINDINGS_6=("0-5" "0-1,4-5,8-9" "0-1,4-5,8,12" "0-1,4,8,12,16" "0,4,8,12,16,20")
# 8 jobs, spread over 2CCL /3CCL /4CCL/ 5CCL /6CCLs:
CPU_BINDINGS_8=("0-7" "0-2,4-5,8-10" "0-1,4-5,8-9,12-13" "0-1,4,8-9,12,16-17" "0-1,4,8,12-13,16,20")
# 8 jobs, spread over 7CCLs/8CCLs:
CPU_BINDINGS_8_8CCL=("0-1,4,8,12,16,20,24" "0,4,8,12,16,20,24,28")
# 12 jobs, spread over 3CCL /4CCL/ 5CCL /6CCLs:
CPU_BINDINGS_12=("0-11" "0-2,4-6,8-10,12-14" "0-1,4-6,8-9,12-14,16-17" "0-1,4-5,8-9,12-13,16-17,20-21")
# 12 jobs, spread over 7CCLs/8CCLs:
CPU_BINDINGS_12_8CCL=("0-1,4-5,8,12-13,16-17,20,24-25" "0-1,4,8-9,12,16-17,20,24-25,28")
# 16 jobs, spread over 4CCL/ 5CCL /6CCLs:
CPU_BINDINGS_16=("0-15" "0-2,4-6,8-10,12-14,16-19" "0-2,4-5,8-10,12-14,16-17,20-22")
# 16 jobs, spread over 7CCLs/8CCLs:
CPU_BINDINGS_16_8CCL=("0-2,4-5,8-9,12-13,16-18,20-21,24-25" "0-1,4-5,8-9,12-13,16-17,20-21,24-25,28-29")
# 20 jobs, spread over 5CCL /6CCLs:
CPU_BINDINGS_20=("0-19" "0-3,4-6,8-10,12-15,16-18,20-22")
# 20 jobs, spread over 7CCLs/8CCLs:
CPU_BINDINGS_20_8CCL=("0-2,4-6,8-10,12-14,16-18,20-22,24-25" "0-2,4-5,8-10,12-13,16-18,20-21,24-26,28-29")
# 24 jobs, spread over 6CCLs:
CPU_BINDINGS_24=("0-23")
# 24 jobs, spread over 7CCLs/8CCLs:
CPU_BINDINGS_24_8CCL=("0-3,4-6,8-11,12-14,16-19,20-22,24-26" "0-2,4-6,8-10,12-14,16-18,20-22,24-26,28-30")
# 28 jobs, spread over 7CCLs/8CCLs:
CPU_BINDINGS_28_8CCL=("0-27" "0-2,4-7,8-10,12-15,16-18,20-23,24-26,28-31")
# 32 jobs, spread over 8CCLs:
CPU_BINDINGS_32_8CCL=("0-31")

# cross NUMAs
# 4 jobs, spread over 1NUMA/ 2NUMA /3NUMA /4NUMA:
CPU_BINDINGS_4_NUMA=("0,4,8,12" "0,4,24,28" "0,4,24,48" "0,24,48,72")
# 8 jobs, spread over 1NUMA/ 2NUMA /3NUMA /4NUMA:
CPU_BINDINGS_8_NUMA=("0-1,4-5,8-9,12-13" "0-1,4-5,24-25,28-29" "0-1,4,24-25,28,48-49" "0-1,24-25,48-49,72-73")
# 12 jobs, spread over 1NUMA/ 2NUMA /3NUMA /4NUMA:
CPU_BINDINGS_12_NUMA=("0-1,4-5,8-9,12-13,16-17,20-21" "0-1,4-5,8-9,24-25,28-29,32-33" "0-1,4-5,24-25,28-29,48-49,52-53" "0,4,8,24,28,32,48,52,56,72,76,80")
# 24 jobs, spread over 1NUMA/ 2NUMA /3NUMA /4NUMA:
CPU_BINDINGS_24_NUMA=("0-23" "0-1,4-5,8-9,12-13,16-17,20-21,24-25,28-29,32-33,36-37,40-41,44-45" "0-1,4-5,8-9,12-13,24-25,28-29,32-33,36-37,48-49,52-53,56-57,60-61" "0-1,4-5,8-9,24-25,28-29,32-33,48-49,52-53,56-57,72-73,76-77,80-81")
# 48 jobs, spread over 2NUMA /3NUMA /4NUMA:
CPU_BINDINGS_48_NUMA=("0-47" "0-2,4-5,8-10,12-14,16-17,20-22,24-26,28-29,32-34,36-38,40-41,44-46,48-50,52-53,56-58,60-62,64-65,68-70" "0-1,4-5,8-9,12-13,16-17,20-21,24-25,28-29,32-33,36-37,40-41,44-45,48-49,52-53,56-57,60-61,64-65,68-69,72-73,76-77,80-81,84-85,88-89,92-93")
# 72 jobs, spread over 3NUMA /4NUMA:
CPU_BINDINGS_72_NUMA=("0-71" "0-2,4-6,8-10,12-14,16-18,20-22,24-26,28-30,32-34,36-38,40-42,44-46,48-50,52-54,56-58,60-62,64-66,68-70,72-74,76-78,80-82,84-86,88-90,92-94")

# pipe pairs CCL0..CCL5
CPU_PAIR_0=("1,0" "1,4" "1,8" "1,12" "1,16" "1,20" "0,0")
CPU_PAIR_1=("5,0" "5,4" "5,8" "5,12" "5,16" "5,20" "4,4")
CPU_PAIR_2=("9,0" "9,4" "9,8" "9,12" "9,16" "9,20" "8,8")
CPU_PAIR_3=("13,0" "13,4" "13,8" "13,12" "13,16" "13,20" "12,12")
CPU_PAIR_4=("17,0" "17,4" "17,8" "17,12" "17,16" "17,20" "16,16")
CPU_PAIR_5=("21,0" "21,4" "21,8" "21,12" "21,16" "21,20" "20,20")


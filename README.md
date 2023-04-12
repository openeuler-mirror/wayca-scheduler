# wayca-scheduler

## Introduction

Wayca-scheduler provides some simple functions for the users to discover the
topology, devices, interrupts and related stuffs like memory characteristics
on the system. It is assumed to be used on the Linux only. Most functions are
still experimental so should be used at your own risk.

You may need to know these hardware details for better optimize your application
since, take kernel scheduler as an example, it's not smart enough to use the
characteristics of the hardware resources like:

- It isn't fully aware of the IO nodes programs are accessing.
- It has no idea if programs are sensitive to cache locality or to memory bandwidth.
- It is trying to spread tasks to more nodes and more CPUs to achieve load balance.
- It has no idea of CPU clusters or other physical topologies.

## Components

Wayca-scheduler includes a library provides the APIs to export topology information,
NUMA information and some simple functions to achieve interrupt binding, memory
policy setting and binding the threads according to the CPU topologies. There are
also some tiny userspace tools for displaying the hardware topology and measuring
the memory characteristics like bandwidth and latency between different topology
and NUMA nodes.

### libwaycascheduler

libwaycascheduler is a shared library providing the basic support of hardware
topologies and some binding functions for other tools in this project and other
linked applications. The APIs can be classified in below types. You can check
wayca-scheduler.h for more detailed information.

- retrieve CPU topology and cache information

Use this set of functions to get topology ID, CPU mask of certain CPU or topology
and cache information of certain CPU. Examples below:

```C
/* How many CPUs in a CCL (CPU cluster) */
int wayca_sc_cpus_in_ccl(void);
/* How many CCLs in a package */
int wayca_sc_ccls_in_package(void);
/* CPU mask in CCL with ID @ccl_id */
int wayca_sc_ccl_cpu_mask(int ccl_id, size_t cpusetsize, cpu_set_t *mask);
/* The CCL ID of CPU with ID @cpu_id */
int wayca_sc_get_ccl_id(int cpu_id);
/* The L3 cache size of CPU with ID @cpu_id */
int wayca_sc_get_l3_size(int cpu_id);
```

- retrieve NUMA and memory topology retrieving

Use this set of functions to get NUMA nodes information including CPU mask and
memory size information. Examples below:

```C
/* How many nodes in a package */
int wayca_sc_nodes_in_package(void);
/* Node mask in package *package_id */
int wayca_sc_package_node_mask(int package_id, size_t setsize, cpu_set_t *mask);
/* Memory size of NUMA node @node_id */
int wayca_sc_get_node_mem_size(int node_id, unsigned long *size);
```

- retrieve and interrupts and devices information

Use this set of functions to get interrupts and devices information. Each interrupt
is described by `struct wayca_sc_irq_info` including the number, type and name
of this IRQ and also the IRQ chip it belongs to. Each device is described by
`struct wayca_sc_device_info` including the name, type, numa nodes and other
detailed information. Examples below:

```C
/* Get detailed information of interrupt with @irq_num */
int wayca_sc_get_irq_info(uint32_t irq_num, struct wayca_sc_irq_info *irq_info);
/* Get detailed information of device with @name */
int wayca_sc_get_device_info(const char *name, struct wayca_sc_device_info *dev_info);
```

- interrupt binding set and retrieval

Use this set of functions to get the current binding information of certain
interrupt or bind it on certain CPU. Examples below:

```C
/* Bind the interrupt @irq to target @cpu */
int wayca_sc_irq_bind_cpu(int irq, int cpu);
/* Get the cpus to which the interrupt @irq is binding */
int wayca_sc_get_irq_bind_cpu(int irq, size_t cpusetsize, cpu_set_t *cpuset);
```

- memory binding policy

Use this set of functions to change the memory binding policy (NUMA policy) of
certain thread. Examples below:

```C
/* Make the allocation of current thread to interleave in targe @package */
int wayca_sc_mem_interleave_in_package(int package);
/* Make the allocation of current thread to target @node */
int wayca_sc_mem_bind_node(int node);
```

- thread binding policy

libwaycascheduler also support to create thread or threadpools which can be
grouped with certain affinity policies. They can be spread across certain
topology level specified to gain better bandwidth or compact with each other
in certain topology levels to gain better latency. Each thread created by
libwaycascheduler has a unique thread id `wayca_sc_thread_t` and each thread
group with a unique group id `wayca_sc_group_t`. The affinity policy is applied
on the thread group and the policy can be described with `wayca_sc_group_attr_t`.
See the comment of `wayca_sc_group_attr_t` for more detailed information.
A wayca thread which is not attached to any group yet is just a simple pthread.

### wayca-sc-info

wayca-sc-info is a simple userspace tool based on libwaycascheduler and provides
the CPU topology, NUMA memory, interrupts and devices information in the system.
By default it retrieves the information from current running system but also
support to store/retrieve the information from certain file with XML format.

### wayca-memory-bench

wayca-memory-bench can be used to measure the bandwidth and latency of certain
memory hierarchy like cache or DRAM and between certain CPUs or topology, for
example the latency between the cluster or the NUMA nodes.

### wayca-calibration

wayca-calibration is an automatic calibration tool used to automate performance
testing and export the results to an XML which could be used by wayca-lstopo.
Currently it supports to measure the memory latency and bandwidth between
different CPUs, clusters, NUMA nodes. It automate the measurement process
depends on other tools to get the data like the stream or wayca-memory-bench
mentioned above.

### wayca-lstopo

wayca-lstopo is an wrapper for hwloc's lstopo-no-graphics. This wrapper adds
support for displaying performance data measured from wayca-calibration of
this system.

## Build & Installation

The project can be build using cmake:

mkdir build && cd build
cmake ..
make
make install

By default this will be installed under /usr/include/, /usr/bin/ and /usr/lib/.
It can be installed to an alternative path with -DCMAKE_INSTALL_PREFIX option
like:

cmake -DCMAKE_INSTALL_PREFIX=${PATH} ../

Enable debug informations with -DDEBUG option and -DASAN in addition if you
want to find potential memory issues.

## LICENSE

The project is licensed under Mulan PSL v2. See the LICENSE file for detailed
information.

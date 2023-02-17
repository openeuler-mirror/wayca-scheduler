/*
 * Copyright (c) 2021 HiSilicon Technologies Co., Ltd.
 * Wayca scheduler is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 */

#ifndef WAYCA_SCHEDULER_H
#define WAYCA_SCHEDULER_H

#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

/**
 * wayca_sc_irq_bind_cpu - bind a specific IRQ to the target cpu
 * @irq: IRQ number
 * @cpu: target cpu
 *
 * This function will change the cpu affinity of the @irq by
 * modifying /proc/irq/<@irq>/smp_affinity.
 *
 * Return 0 if bind successfully and a negative error number
 * if failed to bind.
 */
int wayca_sc_irq_bind_cpu(int irq, int cpu);

/**
 * wayca_sc_get_irq_bind_cpu - get the cpuset a specific IRQ is bound to
 * @irq: IRQ number
 * @cpusetsize: the size of the @cpuset
 * @cpuset: the cpuset pointer to receive the result
 *
 * This function will get the cpu affinity of the @irq by reading
 * /proc/irq/<@irq>/smp_affinity.
 *
 * Return 0 if the cpuset is retrieved successfully and a negtive
 * error number if failed to retrieve the affinity information of
 * the target IRQ.
 */
int wayca_sc_get_irq_bind_cpu(int irq, size_t cpusetsize, cpu_set_t *cpuset);

/* Leverage the bitmap of cpu_set_t, representing the mask of NUMA nodes */
#define node_set_t cpu_set_t
#define NODE_ZERO CPU_ZERO
#define NODE_SET  CPU_SET
#define NODE_ISSET CPU_ISSET

/**
 * wayca_sc_mem_interleave_in_package - make the memory allocation of current
 *                                      thread and its child to interleave in
 *                                      the target package
 * @package: the target package ID
 *
 * This is a wrapper of syscall SYS_set_mempolicy to set the mempolicy
 * of current thread to MPOL_INTERLEAVE and interleave across the nodes
 * of target package.
 *
 * Return 0 if success and a negative error number if failed to
 * change the memory allocation policy of current thread.
 */
int wayca_sc_mem_interleave_in_package(int package);

/**
 * wayca_sc_mem_interleave_in_all - make the memory allocation of current
 *                                  thread and its child to interleave in
 *                                  the target package
 *
 * This is a wrapper of syscall SYS_set_mempolicy to set the mempolicy
 * of current thread to MPOL_INTERLEAVE and interleave across all the nodes
 * in the system.
 *
 * Return 0 if success and a negative error number if failed to
 * change the memory allocation policy of current thread.
 */
int wayca_sc_mem_interleave_in_all(void);

/**
 * wayca_sc_mem_bind_node - make the memory allocation of current thread and
 *                          its child restricted to the target node.
 * @node: the target node ID
 *
 * This is a wrapper of syscall SYS_set_mempolicy to set the mempolicy
 * of current thread to MPOL_BIND and restrict the allocation to the
 * target node.
 *
 * Return 0 if success and a negative error number if failed to
 * change the memory allocation policy of current thread.
 */
int wayca_sc_mem_bind_node(int node);

/**
 * wayca_sc_mem_bind_package - make the memory allocation of current thread
 *                             and its child restricted to the nodes in the
 *                             target package.
 * @package: the target package ID
 *
 * This is a wrapper of syscall SYS_set_mempolicy to set the mempolicy
 * of current thread to MPOL_BIND and restrict the allocation to the
 * nodes in the target package.
 *
 * Return 0 if success and a negative error number if failed to
 * change the memory allocation policy of current thread.
 */
int wayca_sc_mem_bind_package(int package);

/**
 * wayca_sc_mem_unbind - reset the memory allocation policy of current thread
 *
 * This is a wrapper of syscall SYS_set_mempolicy to set the mempolicy
 * of current thread to MPOL_DEFAULT.
 *
 * Return 0 if success and a negative error number if failed to
 * change the memory allocation policy of current thread.
 */
int wayca_sc_mem_unbind(void);

/**
 * wayca_sc_get_mem_bind_nodes - get the allocation nodes of current thread
 * @maxnode: the maximum node ID @mask can receive plus one
 * @mask: the node_set_t pointer to receive the result
 *
 * This is a wrapper of syscall SYS_get_mempolicy to retrieve the nodes
 * which the memory allocation of current thread is interleaved or
 * restricted to.
 *
 * Return 0 if success, -ENODATA if the memory policy of current thread
 * is not MPOL_BIND or MPOL_INTERLEAVE, otherwise a negative error
 * number if failed.
 */
int wayca_sc_get_mem_bind_nodes(size_t maxnode, node_set_t *mask);

/**
 * wayca_sc_mem_migrate_to_node - migrate the pages of certain process to
 *                                target node
 * @pid: pid of target process
 * @node: target node ID
 *
 * This is a wrapper of syscall SYS_migrate_pages to migrate the pages of
 * target process to the target node.
 *
 * Return the number of pages that cannot be migrated, 0 if all the pages
 * of the target process has been migrated successfully and a negative
 * error number on error.
 */
long wayca_sc_mem_migrate_to_node(pid_t pid, int node);

/**
 * wayca_sc_mem_migrate_to_package - migrate the pages of certain process to
 *                                   target package
 * @pid: pid of target process
 * @package: target package ID
 *
 * This is a wrapper of syscall SYS_migrate_pages to migrate the pages of
 * target process to the nodes in the target package.
 *
 * Return the number of pages that cannot be migrated, 0 if all the pages
 * of the target process has been migrated successfully and a negative
 * error number on error.
 */
long wayca_sc_mem_migrate_to_package(pid_t pid, int package);

/**
 * The following family of functions retrieve the topology information
 * of the system.
 *
 * wayca_sc_cpus_in_*(void) returns the number of cpus in the following
 * topology structure, and negative error number on error:
 * core: cpu core which contains multi-threads. for non-SMT system the
 *       core number equals to the cpu number
 * ccl: cpu cluster which shares L3 Tag
 * node: NUMA node
 * package: cpu socket
 * total: cpus in the system
 */
int wayca_sc_cpus_in_core(void);
int wayca_sc_cpus_in_ccl(void);
int wayca_sc_cpus_in_node(void);
int wayca_sc_cpus_in_package(void);
int wayca_sc_cpus_in_total(void);

/*
 * wayca_sc_ccls_in_*(void) returns the number of cpu clusters in the
 * each topology structure, and negative error number on error.
 */
int wayca_sc_ccls_in_package(void);
int wayca_sc_ccls_in_node(void);
int wayca_sc_ccls_in_total(void);

/*
 * wayca_sc_cores_in_*(void) returns the number of cpu cores in the
 * each topology structure, and negative error number on error.
 */
int wayca_sc_cores_in_ccl(void);
int wayca_sc_cores_in_node(void);
int wayca_sc_cores_in_package(void);
int wayca_sc_cores_in_total(void);

/*
 * wayca_sc_nodes_in_*(void) returns the number of NUMA nodes in the
 * each topology structure, and negative error number on error.
 */
int wayca_sc_nodes_in_package(void);
int wayca_sc_nodes_in_total(void);

/*
 * wayca_sc_packages_in_total(void) returns the number of packages in
 * the system, and negative error number on error.
 */
int wayca_sc_packages_in_total(void);

/*
 * The following family of functions retrieve the cpuset mask of certain
 * topology structure.
 *
 * wayca_sc_*_cpu_mask() retrieve the cpuset mask of a certain topology
 * structure ID including
 * @{core, ccl, node, package}_id: ID of the topology structure
 * @cpusetsize: size of @mask
 * @mask: the cpuset pointer to receive the result
 *
 * Return 0 on success and a negative error number on failure.
 */
int wayca_sc_core_cpu_mask(int core_id, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_ccl_cpu_mask(int ccl_id, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_node_cpu_mask(int node_id, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_package_cpu_mask(int package_id, size_t cpusetsize, cpu_set_t *mask);

/**
 * wayca_sc_total_cpu_mask - retrieve the mask for all the cpus in the system
 */
int wayca_sc_total_cpu_mask(size_t cpusetsize, cpu_set_t *mask);

/**
 * wayca_sc_total_online_cpu_mask - retrieve the mask for online cpus in the system
 * Return 0 on success and a negative error number on failure.
 */
int wayca_sc_total_online_cpu_mask(size_t cpusetsize, cpu_set_t *mask);

/**
 * wayca_sc_ccl_core_mask - retrieve the core mask in a certain cluster
 * @ccl_id: target cluster ID
 * @setsize: size of @mask
 * @mask: the core mask to receive the result
 *
 * Return 0 on success and a negative error number on failure.
 */
int wayca_sc_ccl_core_mask(int ccl_id, size_t setsize, cpu_set_t *mask);

/**
 * wayca_sc_node_core_mask - retrieve the core mask in a certain node
 * @node_id: target node ID
 * @setsize: size of @mask
 * @mask: the core mask to receive the result
 *
 * Return 0 on success and a negative error number on failure.
 */
int wayca_sc_node_core_mask(int node_id, size_t setsize, cpu_set_t *mask);

/**
 * wayca_sc_node_ccl_mask - retrieve the cluster mask in a certain node
 * @node_id: target node ID
 * @setsize: size of @mask
 * @mask: the cluster mask to receive the result
 *
 * Return 0 on success and a negative error number on failure.
 */
int wayca_sc_node_ccl_mask(int node_id, size_t setsize, cpu_set_t *mask);

/**
 * wayca_sc_package_node_mask - retrieve the node mask in a certain package
 * @package_id: target package ID
 * @setsize: size of @mask
 * @mask: the node mask to receive the result
 *
 * Return 0 on success and a negative error number on failure.
 */
int wayca_sc_package_node_mask(int package_id, size_t setsize, cpu_set_t *mask);

/**
 * wayca_sc_total_node_mask - retrieve the node mask in the system
 * @setsize: size of @mask
 * @mask: the node mask to receive the result
 *
 * Return 0 on success and a negative error number on failure.
 */
int wayca_sc_total_node_mask(size_t setsize, cpu_set_t *mask);

/**
 * The following family of functions retrieve the topology structure
 * ID which the target cpu belongs to.
 * @cpu_id: the target cpu
 *
 * Return the topology structure ID on success, or a negative error
 * number.
 */
int wayca_sc_get_core_id(int cpu_id);
int wayca_sc_get_ccl_id(int cpu_id);
int wayca_sc_get_node_id(int cpu_id);
int wayca_sc_get_package_id(int cpu_id);

/**
 * The following family of functions retrieve size of specific level
 * cache of a certain cpu.
 * @cpu_id: the target cpu ID
 *
 * The levels of cache includes
 * l1d: L1 data cache
 * l1i: L1 instruction cache
 * l2: L2 cache
 * L3: L3 cache
 *
 * Return the cache size on success, or a negative error number on
 * failure.
 */
int wayca_sc_get_l1d_size(int cpu_id);
int wayca_sc_get_l1i_size(int cpu_id);
int wayca_sc_get_l2_size(int cpu_id);
int wayca_sc_get_l3_size(int cpu_id);

/**
 * wayca_sc_get_node_mem_size - get the memory size on a certain NUMA node
 * @node: node ID
 * @size: pointer to receive the memory size
 *
 * Return 0 on success, or a negative error number on failure.
 */
int wayca_sc_get_node_mem_size(int node_id, unsigned long *size);

/* The type of the interrupt */
enum wayca_sc_irq_type {
	WAYCA_SC_TOPO_TYPE_INVAL,
	WAYCA_SC_TOPO_TYPE_EDGE,
	WAYCA_SC_TOPO_TYPE_LEVEL,
};

/* The name of the IRQ controller chip */
enum wayca_sc_irq_chip_name {
	WAYCA_SC_TOPO_CHIP_NAME_INVAL,
	WAYCA_SC_TOPO_CHIP_NAME_MBIGENV2,
	WAYCA_SC_TOPO_CHIP_NAME_ITS_MSI,
	WAYCA_SC_TOPO_CHIP_NAME_ITS_PMSI,
	WAYCA_SC_TOPO_CHIP_NAME_GICV3,
};

/**
 * struct wayca_sc_irq_info - IRQ information descriptor
 * @irq_num: the number of the interrupt
 * @chip_name: the IRQ chip name this interrupt belongs to
 * @type: the type of this interrupt
 * @name: the registered name of this interrupt, maybe void
 *        if this interrupt is inactive
 */
struct wayca_sc_irq_info {
	unsigned long irq_num;
	enum wayca_sc_irq_chip_name chip_name;
	enum wayca_sc_irq_type type;
	const char *name;
};

/**
 * wayca_sc_get_irq_list - get the IRQ number list in the system
 * @num: the element number in the list
 * @irq: the array of the IRQs
 *
 * The IRQ list length is returned in @num and the IRQs are
 * returns in the array @irq. The caller should make sure
 * the @irq array have enough memory to save @num IRQs.
 * The caller can get the length first by passing NULL to @irq,
 * and then get the whole list by passing both @irq and @num.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_get_irq_list(size_t *num, uint32_t *irq);

/**
 * wayca_sc_get_irq_info - get the information of certain IRQ
 * @irq_num: the target IRQ number
 * @irq_info: the returned information of the target IRQ
 *
 * The detailed information of certain IRQ @irq_num is returned
 * through @irq_info. The memory of @irq_info should be
 * allocated and maintained by the caller.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_get_irq_info(uint32_t irq_num, struct wayca_sc_irq_info *irq_info);

/* The type of the device */
enum wayca_sc_device_type {
	WAYCA_SC_TOPO_DEV_TYPE_INVAL,
	WAYCA_SC_TOPO_DEV_TYPE_PCI,
	WAYCA_SC_TOPO_DEV_TYPE_SMMU,
};

/**
 * struct wayca_sc_device_info - device information descriptor
 * @name: the name of the device
 * @dev_type: the type of the device
 * @smmu_idx: the smmu device index which this device belongs to
 * @numa_node: the NUMA node ID which this device belongs to
 * @base_addr: the base of address of SMMU device
 * @modalias: the alias name of the device
 * @device: the device ID of PCI/PCIe device
 * @vendor: the vendor ID of PCI/PCIe device
 * @class: the class ID of PCI/PCIe device
 * @irq_numbers: the IRQ number array of the device
 * @nb_irq: the number of the IRQs in the IRQ number array
 */
struct wayca_sc_device_info {
	const char *name; /* name which used to find the device in wayca_sc */
	enum wayca_sc_device_type dev_type;
	int smmu_idx;
	int numa_node;
	union {
		struct {
			uint64_t base_addr;
			const char *modalias;
		};
		struct {
			uint16_t device;
			uint16_t vendor;
			uint32_t class;
			const uint32_t *irq_numbers;
			int nb_irq;
		};
	};
};

/**
 * wayca_sc_get_device_list - get the devices' name on certain NUMA node
 *                            or in the system
 * @numa_node: the ID of the NUMA node to query. if node id < 0, then
 *             return all the devices in system
 * @num: the number of the devices
 * @name: the array of the devices' name
 *
 * The name of the devices on the node @numa_node will be return as
 * an array through @name with length @num. The caller should make
 * sure the length of @num can hold all the names of the devices on
 * the node, which means >= @num * sizeof(char *).
 * The memory is allocated and maintained by the caller.
 * Each element in the @num is a pointer of name referenced to the
 * name maintained by the library, so caller don't need to allocate
 * the memory of each element.
 *
 * User can call this function firstly with a NULL @name to get the
 * number of the devices on the node, than allocates the memory of
 * @name and call this function to get the list of the devices' name
 * on the node.
 *
 * Return 0 on success, or a negative error number on failure.
 */
int wayca_sc_get_device_list(int numa_node, size_t *num, const char **name);

/**
 * wayca_sc_get_device_info - get the detailed information by device name
 * @name: the name of the device
 * @dev_info: the information of the device
 *
 * Get the device's detailed information returned by @dev_info. The memory
 * of @dev_info should be allocated and maintained by the caller.
 *
 * Return 0 on success and a negative error number on failure.
 */
int wayca_sc_get_device_info(const char *name, struct wayca_sc_device_info *dev_info);

int wayca_managed_thread_create(int id, pthread_t *thread, const pthread_attr_t *attr,
				void *(*start_routine) (void *), void *arg);

int wayca_managed_threadpool_create(int id, int num, pthread_t *thread[],
				    const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);

/*
 * The identifier of wayca scheduler thread
 *
 * The maximum wayca scheduler threads user can created simultaneously
 * is default to 32765. It can be modified by passing the desired
 * upper limits to environment variable WAYCA_SC_THREADS_NUMBER.
 */
typedef unsigned long long	wayca_sc_thread_t;

/* Deprecated. The attribute of wayca scheduler thread */
typedef unsigned long long	wayca_sc_thread_attr_t;
/* Deprecated. Definition of wayca thread attributes */
#define WT_TF_WAYCA_MANAGEABLE	0x10000

/* Deprecated. DO NOT USE. */
int wayca_sc_thread_set_attr(wayca_sc_thread_t wthread, wayca_sc_thread_attr_t *attr);
int wayca_sc_thread_get_attr(wayca_sc_thread_t wthread, wayca_sc_thread_attr_t *attr);

/**
 * wayca_sc_thread_create - create a wayca scheduler thread
 * @wthread: the identifier of the wayca scheduler thread created
 * @attr: the pthread attribute of the thread's underlaid pthread
 * @start_routine: the function to run in the thread
 * @arg: the argument for @start_routine
 *
 * Return 0 on success, or a negative error number on failure.
 */
int wayca_sc_thread_create(wayca_sc_thread_t *wthread, pthread_attr_t *attr,
			   void *(*start_routine)(void *), void *arg);

/**
 * wayca_sc_thread_join - join with a terminated wayca scheduler thread
 * @wthread: the identifier of the thread to join
 * @retval: pointer to hold the return value of the thread function
 *
 * Destroy a wayca scheduler thread until the thread is terminated.
 * If the thread has been attached to a wayca scheduler group,
 * the function will detach it first.
 *
 * Return 0 on success, or a negative error number on failure.
 */
int wayca_sc_thread_join(wayca_sc_thread_t wthread, void **retval);

/**
 * wayca_sc_thread_kill - send signal to a wayca scheduler thread
 * @wthread: the identifier of the thread
 * @sig: the signal number to be sent
 *
 * Return 0 on success, or a negative error number on failure.
 */
int wayca_sc_thread_kill(wayca_sc_thread_t wthread, int sig);

/**
 * wayca_sc_pid_attach_thread - create a wayca scheduler thread from an
 *                              existed thread or process
 * @wthread: the identifier of the wayca scheduler thread created
 * @pid: the pid of the target thread or process
 *
 * Create a wayca scheduler thread from an existed thread or process,
 * the thread can be managed by the wayca scheduler group as well.
 * If the pid is 0, a wayca scheduler thread will be created for the
 * current thread or process.
 *
 * User should avoid creating wayca scheduler threads multiple times
 * with same pid or wayca scheduler threads. This will leads to
 * undefined behaviour and the library won't check for this.
 *
 * The wayca scheduler thread won't know the termination of the attached
 * thread or process, user should manage the lifetime of the thread
 * or process and the attached wayca scheduler thread.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_pid_attach_thread(wayca_sc_thread_t *wthread, pid_t pid);

/**
 * wayca_sc_pid_detach_thread - destroy a wayca scheduler thread created
 *                              from an existed thread or process
 * @wthread: the wayca scheduler thread to be detached
 *
 * Destroy a wayca scheduler thread which is created by attaching the
 * existed thread or process, and attached process or thread won't
 * be affected.
 *
 * Only the wayca scheduler thread created by wayca_sc_pid_attach_thread()
 * can be destroyed by this function. Try to destroy the thread created
 * by wayca_sc_thread_create() is not allowd and will return failure.
 *
 * Return 0 on success, otherwise a negative error number on failure.
 */
int wayca_sc_pid_detach_thread(wayca_sc_thread_t wthread);

/*
 * The identifier of wayca scheduler group
 *
 * The wayca scheduler group is a set of wayca scheduler threads or
 * wayca scheduler groups. It has the following features:
 *  - the members in the group must have the same type, either
 *    wayca scheduler threads or wayca scheduler groups
 *  - the threads in the group have the same cpu affinity binding
 *    policy, which is decided by the group's attribute as
 *    described below
 *  - the group covers a set of cpus. the cpu affinity of the
 *    member threads always be within the range of the group.
 *    the cpu range of member groups is also a subset of the
 *    father group's cpu range.
 *  - the cpu range of the group is decided by its father group's
 *    attribute, or if it has no father it will cover all the
 *    cpus in the system
 *
 * The maximum wayca scheduler groups user can created simultaneously
 * is default to 256. It can be modified by passing the desired
 * upper limits to environment variable WAYCA_SC_GROUPS_NUMBER.
 */
typedef unsigned long long	wayca_sc_group_t;

/**
 * The attribute of wayca scheduler group
 *
 * The attribute of wayca scheduler group will determine the
 * granularity of the members and if member type is thread,
 * it'll also affect the binding style and relationship between
 * the member threads.
 *
 * There are three types of attribute in different bitfield of
 * wayca_sc_group_attr_t:
 * Bit[0:15]: The topology granularity of each members in this group, which
 *            will determine the cpu range assigned to each members from
 *            the father group
 * Bit[16:19]: Group thread's binding style, per-CPU or not
 * Bit[20:32]: Group thread's relationship, whether the member threads bind
 *             as sparsely as possible or not.
 *
 * The binding style and relationship only affects thread members.
 *
 * Considering the father group's range spans one NUMA, with 4 clusters and
 * 4 cpus in each cluster, the group attribute is WT_GF_CCL.
 * If the members are groups, then the range of each group will be like:
 *        [                       Father Group                      ]
 * range:                             0 - 15
 *        [ Group 0 ] [ Group 1 ] [ Group 2 ] [ Group 3 ] [ Group 4 ]
 * range:    0 - 3       4 - 7       8 - 11      12 - 15     0 - 3
 *
 * If the members are threads, then
 *           [                          Father Group                        ]
 * range:                                   0 - 15
 *           [ Thread 0 ] [ Thread 1 ] [ Thread 2 ] [ Thread 3 ] [ Thread 4 ]
 * affinity:     0 - 3        4 - 7       8 - 11      12 - 15        0 - 3   WT_GF_CCL
 * affinity:       0            4           8            12            1     WT_GF_CCL|WT_GF_PERCPU
 * affinity:     0 - 3        0 - 3       0 - 3        0 - 3         4 - 7   WT_GF_CCL|WT_GF_COMPACT
 * affinity:       0            1           2            3             4     WT_GF_CCL|WT_GF_PERCPU|
 *                                                                           WT_GF_COMPACT
 */
typedef unsigned long long	wayca_sc_group_attr_t;
#define WT_GF_CPU	0x00000001	/* Each thread/group accepts per-CPU affinity */
#define WT_GF_CCL	0x00000004	/* Each thread/group accepts per-CCL affinity */
#define WT_GF_NUMA	0x00000020	/* Each thread/group accepts per-NUMA affinity */
#define WT_GF_PACKAGE	0x00000040	/* Each thread/group accepts per-Package affinity */
#define WT_GF_ALL	0x00000400	/* Each thread/group doesn't have an affinity hint */
#define WT_GF_PERCPU	0x00010000	/* Each thread will bind to the CPU */
#define WT_GF_COMPACT	0x00100000	/* The threads in this group will be compact */

/**
 * wayca_sc_group_set_attr - set the attribute of wayca scheduler group
 * @group: the target wayca scheduler group
 * @attr: the attribute to be set
 *
 * Change the attribute of target wayca scheduler group @group to @attr.
 * Caller can get the actual attribute of the group though @attr when the
 * function returned.
 *
 * The cpu affinity of the members in the group will be changed according
 * to the @attr recursively, which means that if the @group is a group of
 * wayca scheduler groups, the cpu affinity of the members of these
 * member groups will be changed as well as their father group's attribute
 * has been changed.
 *
 * Return 0 on succes, otherwise a negative error number on failure.
 */
int wayca_sc_group_set_attr(wayca_sc_group_t group, wayca_sc_group_attr_t *attr);

/**
 * wayca_sc_group_get_attr - get the attribute of wayca scheduler group
 * @group: the target wayca scheduler group
 * @attr: the attribute of the group
 *
 * Get the current attribute of the target wayca scheduler group @group.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_group_get_attr(wayca_sc_group_t group, wayca_sc_group_attr_t *attr);

/**
 * wayca_sc_group_create - create a wayca scheduler group
 * @group: the identifier of the wayca scheduler group created
 *
 * Created an empty wayca scheduler group with default attribute
 * WT_GF_CPU | WT_GF_COMPACT | WT_GF_PERCPU. The group identifier
 * created will be return by @group.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_group_create(wayca_sc_group_t *group);

/**
 * wayca_sc_group_destroy - destroy a wayca scheduler group
 * @group: the identifier of the wayca scheduler group to be destroyed
 *
 * Destroy a wayca scheduler group. The caller should make sure the group
 * is empty before destroying. If the group is non-empty, which means it
 * still has thread(s)/group(s) attached, it cannot be destroyed and
 * -EBUSY is returned.
 *
 * If the wayca scheduler group has been attached to a father group,
 * the function will detach it first.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_group_destroy(wayca_sc_group_t group);

/**
 * wayca_sc_thread_attach_group - attach a wayca scheduler thread to the
 *                                target wayca scheduler group
 * @wthread: the wayca scheduler thread to be attached
 * @group: the target wayca scheduler group
 *
 * Attach the wayca scheduler thread @wthread to the target group.
 * The cpu affinity of the thread will be changed according to
 * attribute of the group when the call returns.
 *
 * Only thread within no group can be attached, otherwise it should
 * be detached from the old group first before attaching to a new one.
 *
 * A thread cannot be attached to a group which members are wayca scheduler
 * group(s). The group members should always be the same type, either
 * thread(s) or group(s), otherwise the operation will fail.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_thread_attach_group(wayca_sc_thread_t wthread, wayca_sc_group_t group);

/**
 * wayca_sc_thread_detach_group - detach a wayca scheduler thread from the
 *                                target wayca scheduler group
 * @wthread: the wayca scheduler thread to be attached
 * @group: the target wayca scheduler group
 *
 * Detach the member thread @wthread from the target group @group.
 * The cpu affinity of the thread will remained unchanged after detachment.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_thread_detach_group(wayca_sc_thread_t wthread, wayca_sc_group_t group);

/**
 * wayca_sc_group_attach_group - attach a wayca scheduler group to the target
 *                               wayca scheduler group
 * @group: the wayca scheduler group to be attached
 * @father: the target wayca scheduler group
 *
 * Attach the wayca scheduler group @group to the target group.
 * The cpu range managed by the @group will be changed according to
 * attribute of the father group when the call returns.
 *
 * Only group within no father group can be attached, otherwise it should
 * be detached from the old group first before attaching to a new one.
 *
 * A group cannot be attached to a group which members are wayca scheduler
 * thread(s). The group members should always be the same type, either
 * thread(s) or group(s), otherwise the operation will fail.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_group_attach_group(wayca_sc_group_t group, wayca_sc_group_t father);

/**
 * wayca_sc_group_detach_group - detach a wayca scheduler group from the
 *                               target wayca scheduler group
 * @group: the wayca scheduler group to be detached
 * @father: the target wayca scheduler group
 *
 * Detach the member group @group from the father group @father.
 * The cpu range managed by the @group will remained unchanged after detachment.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_group_detach_group(wayca_sc_group_t group, wayca_sc_group_t father);

/**
 * wayca_sc_is_thread_in_group - whether the wayca scheduler thread is in
 *                               the wayca scheduler group
 * @thread: the wayca scheduler thread to be judged
 * @group: the target wayca scheduler group
 *
 * Return
 * - 0: the thread is not in the group
 * - > 0: the thread is in the group
 * - -EINVAL: invalid thread or group identifier
 */
int wayca_sc_is_thread_in_group(wayca_sc_thread_t thread, wayca_sc_group_t group);

/**
 * wayca_sc_is_group_in_group - whether the wayca scheduler group is in
 *                               the wayca scheduler group
 * @target: the wayca scheduler group to be judged
 * @group: the target wayca scheduler group
 *
 * Return
 * - 0: the group is not in the father group
 * - > 0: the group is in the father group
 * - -EINVAL: invalid group identifier
 */
int wayca_sc_is_group_in_group(wayca_sc_group_t target, wayca_sc_group_t group);

/*
 * The identifier of the wayca scheduler threadpool
 *
 * The maximum wayca scheduler threadpools user can created simultaneously
 * is default to 256. It can be modified by passing the desired
 * upper limits to environment variable WAYCA_SC_THREADPOOLS_NUMBER.
 */
typedef unsigned long long	wayca_sc_threadpool_t;

/* The prototype of the task function accepted by the wayca scheduler threadpool */
typedef void (*wayca_sc_threadpool_task_func)(void *);

/**
 * wayca_sc_threadpool_create - create a wayca scheduler threadpool
 * @threadpool: the identifier of the wayca scheduler threadpool created
 * @attr: the pthread attribute of threads in the pool
 * @num: the number of working threads to be created in the pool
 *
 * Create a wayca scheduler threadpool composed of @num wayca scheduler
 * threads with attribute @attr. A wayca scheduler group is also created
 * and all the work threads will be attached to the group. The attribute
 * of the group is default to WT_GF_CPU | WT_GF_COMPACT | WT_GF_PERCPU.
 *
 * The number of threads can be created depends on the limitation of
 * wayca scheduler threads as well.
 *
 * Return how many threads successfully created in the pool, or a negative
 * error number on failure.
 */
ssize_t wayca_sc_threadpool_create(wayca_sc_threadpool_t *threadpool,
				   pthread_attr_t *attr, size_t num);

/**
 * wayca_sc_threadpool_destroy - destroy a wayca scheduler threadpool
 * @threadpool: the identifier of the wayca scheduler threadpool to destroy
 *
 * Destroy a wayca scheduler threadpool and terminate all the working
 * threads. This function will be blocked until all the tasks already
 * executed to be finished. And any tasks waiting in the queue will be
 * discarded.
 *
 * Return 0 on success, or a negative error number.
 */
int wayca_sc_threadpool_destroy(wayca_sc_threadpool_t threadpool);

/**
 * wayca_sc_threadpool_get_group - get the internal wayca scheduler group
 *                                 of the threadpool
 * @threadpool: the identifier of the wayca scheduler threadpool
 * @group: the wayca scheduler group within the threadpool
 *
 * Get the wayca scheduler group identifier within the target wayca
 * scheduler threadpool.
 *
 * Return 0 on success, or a negative error number.
 */
int wayca_sc_threadpool_get_group(wayca_sc_threadpool_t threadpool,
				  wayca_sc_group_t *group);

/**
 * wayca_sc_threadpool_queue - queue a task into the wayca scheduler threadpool
 * @threadpool: the identifier of the wayca scheduler threadpool
 * @task_func: the function to be executed
 * @arg: the argument of @task_func
 *
 * Queue a task into the threadpool to execute. If there's idle working
 * thread(s) in the pool, the task will be executed immediatedly.
 * Otherwise it will be queued into an internal FIFO waiting for
 * execution.
 *
 * Return 0 on success, or a negative error number.
 */
int wayca_sc_threadpool_queue(wayca_sc_threadpool_t threadpool,
			      wayca_sc_threadpool_task_func task_func, void *arg);

/**
 * wayca_sc_threadpool_thread_num - get the work thread(s) number in the pool
 * @threadpool: the identifier of the wayca scheduler threadpool
 *
 * Return the number of work thread(s) in the threadpool, otherwise
 * a negative error number on failure.
 */
ssize_t wayca_sc_threadpool_thread_num(wayca_sc_threadpool_t threadpool);

/**
 * wayca_sc_threadpool_task_num - get the number of tasks waiting for execution
 * @threadpool: the identifier of the wayca scheduler threadpool
 *
 * Return the number of tasks waiting for execution in the threadpool,
 * otherwise a negative error number on failure.
 */
ssize_t wayca_sc_threadpool_task_num(wayca_sc_threadpool_t threadpool);

/**
 * wayca_sc_threadpool_running_num - get the number of working thread(s) which
 *                                   are(is) not idle
 * @threadpool: the identifier of the wayca scheduler threadpool
 *
 * Return the number of idle thread(s) in threadpool, which means these
 * threads are waiting for tasks to execute. If there're idle threads
 * in the threadpool, there must be no task waiting for execution.
 * A negative error number will be returned on failure.
 */
ssize_t wayca_sc_threadpool_running_num(wayca_sc_threadpool_t threadpool);

/* For debug purpose */
#ifdef WAYCA_SC_DEBUG

/**
 * wayca_sc_topo_print - print the topology information of the system
 *
 * By default the information will be printed to stdout.
 */
void wayca_sc_topo_print(void);

/**
 * wayca_sc_thread_get_cpuset - get the cpu affinity of wayca scheduler thread
 * @wthread: the identifier of wayca scheduler thread
 * @cpusetsize: the size of @cpuset
 * @cpuset: the cpuset mask of @wthread
 *
 * This function retrieves the cpu affinity of @wthread.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_thread_get_cpuset(wayca_sc_thread_t wthread, size_t cpusetsize,
			       cpu_set_t *cpuset);

/**
 * wayca_sc_group_get_cpuset - get the cpuset range that wayca scheduler
 *                             group manages
 * @group: the identifier of wayca scheduler group
 * @cpusetsize: the size of @cpuset
 * @cpuset: the cpuset mask of @group
 *
 * This function retrieves the cpuset mask that @group manages.
 *
 * Return 0 on success, otherwise a negative error number.
 */
int wayca_sc_group_get_cpuset(wayca_sc_group_t group, size_t cpusetsize,
			      cpu_set_t *cpuset);
#else
static inline
void wayca_sc_topo_print(void) { }
static inline
int wayca_sc_thread_get_cpuset(wayca_sc_thread_t wthread, size_t cpusetsize,
			       cpu_set_t *cpuset)
{
	return 0;
}
static inline
int wayca_sc_group_get_cpuset(wayca_sc_group_t group, size_t cpusetsize,
			      cpu_set_t *cpuset)
{
	return 0;
}
#endif /* WAYCA_SC_DEBUG */

#define WAYCA_SC_ATTR_STRING_LEN (256)	/* default attribute string length */
#endif

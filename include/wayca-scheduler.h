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

#include <stdbool.h>

#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

int wayca_sc_irq_bind_cpu(int irq, int cpu);
int wayca_sc_get_irq_bind_cpu(int irq, size_t cpusetsize, cpu_set_t *cpuset);

/* Leverage the bitmap of cpu_set_t */
#define node_set_t cpu_set_t
#define NODE_ZERO CPU_ZERO
#define NODE_SET  CPU_SET
#define NODE_ISSET CPU_ISSET

int wayca_sc_mem_interleave_in_package(int package);
int wayca_sc_mem_interleave_in_all(void);
int wayca_sc_mem_bind_node(int node);
int wayca_sc_mem_bind_package(int package);
int wayca_sc_mem_unbind(void);
int wayca_sc_get_mem_bind_nodes(size_t maxnode, node_set_t *mask);
long wayca_sc_mem_migrate_to_node(pid_t pid, int node);
long wayca_sc_mem_migrate_to_package(pid_t pid, int package);

int wayca_sc_cpus_in_core(void);
int wayca_sc_cpus_in_ccl(void);
int wayca_sc_cpus_in_node(void);
int wayca_sc_cpus_in_package(void);
int wayca_sc_cpus_in_total(void);
int wayca_sc_ccls_in_package(void);
int wayca_sc_ccls_in_node(void);
int wayca_sc_ccls_in_total(void);
int wayca_sc_cores_in_ccl(void);
int wayca_sc_cores_in_node(void);
int wayca_sc_cores_in_package(void);
int wayca_sc_cores_in_total(void);
int wayca_sc_nodes_in_package(void);
int wayca_sc_nodes_in_total(void);
int wayca_sc_packages_in_total(void);

int wayca_sc_core_cpu_mask(int core, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_ccl_cpu_mask(int ccl, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_node_cpu_mask(int node, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_package_cpu_mask(int package, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_total_cpu_mask(size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_package_node_mask(int package, size_t setsize, cpu_set_t *mask);
int wayca_sc_total_node_mask(size_t setsize, cpu_set_t *mask);

int wayca_sc_get_core_id(int cpu);
int wayca_sc_get_ccl_id(int cpu);
int wayca_sc_get_node_id(int cpu);
int wayca_sc_get_package_id(int cpu);

int wayca_sc_get_l1d_size(int cpu_id);
int wayca_sc_get_l1i_size(int cpu_id);
int wayca_sc_get_l2_size(int cpu_id);
int wayca_sc_get_l3_size(int cpu_id);

int wayca_sc_get_node_mem_size(int node, unsigned long *size);
enum wayca_sc_irq_type {
	WAYCA_SC_TOPO_TYPE_INVAL,
	WAYCA_SC_TOPO_TYPE_EDGE,
	WAYCA_SC_TOPO_TYPE_LEVEL,
};

enum wayca_sc_irq_chip_name {
	WAYCA_SC_TOPO_CHIP_NAME_INVAL,
	WAYCA_SC_TOPO_CHIP_NAME_MBIGENV2,
	WAYCA_SC_TOPO_CHIP_NAME_ITS_MSI,
	WAYCA_SC_TOPO_CHIP_NAME_ITS_PMSI,
	WAYCA_SC_TOPO_CHIP_NAME_GICV3,
};

struct wayca_sc_irq_info {
	unsigned long irq_num;
	enum wayca_sc_irq_chip_name chip_name;
	enum wayca_sc_irq_type type;
	const char *name;
};

int wayca_sc_get_irq_list(size_t *num, uint32_t *irq);
int wayca_sc_get_irq_info(uint32_t irq_num, struct wayca_sc_irq_info *irq_info);

enum wayca_sc_device_type {
	WAYCA_SC_TOPO_DEV_TYPE_INVAL,
	WAYCA_SC_TOPO_DEV_TYPE_PCI,
	WAYCA_SC_TOPO_DEV_TYPE_SMMU,
};

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

int wayca_sc_get_device_list(int numa_node, size_t *num, const char **name);
int wayca_sc_get_device_info(const char *name, struct wayca_sc_device_info *dev_info);

int wayca_managed_thread_create(int id, pthread_t *thread, const pthread_attr_t *attr,
				void *(*start_routine) (void *), void *arg);

int wayca_managed_threadpool_create(int id, int num, pthread_t *thread[],
				    const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);

typedef unsigned long long	wayca_sc_thread_t;
typedef unsigned long long	wayca_sc_thread_attr_t;
/* Flags for wayca thread attributes */
#define WT_TF_WAYCA_MANAGEABLE	0x10000

int wayca_sc_thread_set_attr(wayca_sc_thread_t wthread, wayca_sc_thread_attr_t *attr);
int wayca_sc_thread_get_attr(wayca_sc_thread_t wthread, wayca_sc_thread_attr_t *attr);

int wayca_sc_thread_create(wayca_sc_thread_t *wthread, pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int wayca_sc_thread_join(wayca_sc_thread_t wthread, void **retval);
int wayca_sc_thread_kill(wayca_sc_thread_t id, int sig);
int wayca_sc_pid_attach_thread(wayca_sc_thread_t *wthread, pid_t pid);
int wayca_sc_pid_detach_thread(wayca_sc_thread_t wthread);


typedef unsigned long long	wayca_sc_group_t;
typedef unsigned long long	wayca_sc_group_attr_t;
/**
 * Flags for wayca sc group attributes
 * Bit[0:15]: The accepted topology of the thread/group in this group
 * Bit[16:19]: Group thread's binding style
 * Bit[20:32]: Group thread's relationship
 */
#define WT_GF_CPU	1		/* Each thread/group accepts per-CPU affinity */
#define WT_GF_CCL	4		/* Each thread/group accepts per-CCL affinity */
#define WT_GF_NUMA	32		/* Each thread/group accepts per-NUMA affinity */
#define WT_GF_PACKAGE	64		/* Each thread/group accepts per-Package affinity */
#define WT_GF_ALL	1024		/* Each thread/group doesn't have an affinity hint */
#define WT_GF_PERCPU	0x10000		/* Each thread will bind to the CPU */
#define WT_GF_COMPACT	0x100000	/* The threads in this group will be compact */

int wayca_sc_group_set_attr(wayca_sc_group_t group, wayca_sc_group_attr_t *attr);
int wayca_sc_group_get_attr(wayca_sc_group_t group, wayca_sc_group_attr_t *attr);

int wayca_sc_group_create(wayca_sc_group_t *group);
int wayca_sc_group_destroy(wayca_sc_group_t group);
int wayca_sc_thread_attach_group(wayca_sc_thread_t wthread, wayca_sc_group_t group);
int wayca_sc_thread_detach_group(wayca_sc_thread_t wthread, wayca_sc_group_t group);
int wayca_sc_group_attach_group(wayca_sc_group_t group, wayca_sc_group_t father);
int wayca_sc_group_detach_group(wayca_sc_group_t group, wayca_sc_group_t father);
int wayca_sc_is_thread_in_group(wayca_sc_thread_t thread, wayca_sc_group_t group);
int wayca_sc_is_group_in_group(wayca_sc_group_t target, wayca_sc_group_t group);

typedef unsigned long long	wayca_sc_threadpool_t;
typedef void (*wayca_sc_threadpool_task_func)(void *);

ssize_t wayca_sc_threadpool_create(wayca_sc_threadpool_t *threadpool,
				   pthread_attr_t *attr, size_t num);
int wayca_sc_threadpool_destroy(wayca_sc_threadpool_t threadpool);
int wayca_sc_threadpool_get_group(wayca_sc_threadpool_t threadpool, wayca_sc_group_t *group);
int wayca_sc_threadpool_queue(wayca_sc_threadpool_t threadpool,
			   wayca_sc_threadpool_task_func task_func, void *arg);
int wayca_sc_threadpool_thread_num(wayca_sc_threadpool_t threadpool);
/* Get the number of unqueued tasks */
int wayca_sc_threadpool_task_num(wayca_sc_threadpool_t threadpool);
int wayca_sc_threadpool_running_num(wayca_sc_threadpool_t threadpool);

/* For debug purpose */
#ifdef WAYCA_SC_DEBUG
void wayca_sc_topo_print(void);
int wayca_sc_thread_get_cpuset(wayca_sc_thread_t wthread, size_t cpusetsize,
			       cpu_set_t *cpuset);
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

#define WAYCA_SC_ATTR_STRING_LEN	(256)		/* default attribute string length */
#endif

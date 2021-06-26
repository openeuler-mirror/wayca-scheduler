#ifndef WAYCA_SCHEDULER_H
#define WAYCA_SCHEDULER_H

#include <stdbool.h>

#include <pthread.h>
#include <unistd.h>

int wayca_sc_irq_bind_cpu(int irq, int cpu);
int wayca_sc_get_irq_bind_cpu(int irq, size_t cpusetsize, cpu_set_t *cpuset);

int wayca_sc_mem_interleave_in_package(int node);
int wayca_sc_mem_interleave_in_all(void);
int wayca_sc_mem_bind_node(int node);
int wayca_sc_mem_bind_package(int node);
int wayca_sc_mem_unbind(void);
long wayca_sc_mem_migrate_to_node(pid_t pid, int node);
long wayca_sc_mem_migrate_to_package(pid_t pid, int node);

int wayca_sc_cpus_in_ccl(void);
int wayca_sc_cpus_in_node(void);
int wayca_sc_cpus_in_package(void);
int wayca_sc_cpus_in_total(void);
int wayca_sc_ccls_in_package(void);
int wayca_sc_ccls_in_node(void);
int wayca_sc_ccls_in_total(void);
int wayca_sc_nodes_in_package(void);
int wayca_sc_nodes_in_total(void);
int wayca_sc_packages_in_total(void);

int wayca_sc_core_cpu_mask(int core, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_ccl_cpu_mask(int ccl, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_node_cpu_mask(int node, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_package_cpu_mask(int node, size_t cpusetsize, cpu_set_t *mask);
int wayca_sc_total_cpu_mask(size_t cpusetsize, cpu_set_t *mask);

int wayca_sc_get_core_id(int cpu);
int wayca_sc_get_ccl_id(int cpu);
int wayca_sc_get_node_id(int cpu);
int wayca_sc_get_package_id(int cpu);

int wayca_sc_get_node_mem_size(int node, unsigned long *size);

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

int wayca_sc_group_create(wayca_sc_group_t* group);
int wayca_sc_group_destroy(wayca_sc_group_t group);
int wayca_sc_thread_attach_group(wayca_sc_thread_t wthread, wayca_sc_group_t group);
int wayca_sc_thread_detach_group(wayca_sc_thread_t wthread, wayca_sc_group_t group);
int wayca_sc_group_attach_group(wayca_sc_group_t group, wayca_sc_group_t father);
int wayca_sc_group_detach_group(wayca_sc_group_t group, wayca_sc_group_t father);
int wayca_sc_is_thread_in_group(wayca_sc_thread_t thread, wayca_sc_group_t group);
int wayca_sc_is_group_in_group(wayca_sc_group_t target, wayca_sc_group_t group);

/* For debug purpose */
#ifdef WAYCA_SC_DEBUG
void wayca_sc_topo_print(void);
int wayca_sc_thread_get_cpuset(wayca_sc_thread_t wthread, cpu_set_t *cpuset);
int wayca_sc_group_get_cpuset(wayca_sc_group_t group, cpu_set_t *cpuset);
#else
static inline
void wayca_sc_topo_print(void) { }
static inline
int wayca_sc_thread_get_cpuset(wayca_sc_thread_t wthread, cpu_set_t *cpuset)
{
	return 0;
}
static inline
int wayca_sc_group_get_cpuset(wayca_sc_group_t group, cpu_set_t *cpuset)
{
	return 0;
}
#endif /* WAYCA_SC_DEBUG */

#endif

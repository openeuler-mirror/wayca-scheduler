#ifndef WAYCA_SCHEDULER_H
#define WAYCA_SCHEDULER_H

#include <stdbool.h>

#include <pthread.h>
#include <unistd.h>

int thread_bind_cpu(pid_t pid, int cpu);
int thread_bind_ccl(pid_t pid, int cpu);
int thread_bind_node(pid_t pid, int node);
int thread_bind_package(pid_t pid, int node);
int thread_unbind(pid_t pid);
int process_bind_cpu(pid_t pid, int cpu);
int process_bind_ccl(pid_t pid, int cpu);
int process_bind_node(pid_t pid, int node);
int process_bind_package(pid_t pid, int node);
int process_unbind(pid_t pid);

int irq_bind_cpu(int irq, int cpu);

int mem_interleave_in_package(int node);
int mem_interleave_in_all(void);
int mem_bind_node(int node);
int mem_bind_package(int node);
int mem_unbind(void);
long mem_migrate_to_node(pid_t pid, int node);
long mem_migrate_to_package(pid_t pid, int node);

int cores_in_ccl(void);
int cores_in_node(void);
int cores_in_package(void);
int cores_in_total(void);
int nodes_in_package(void);
int nodes_in_total(void);

int wayca_managed_thread_create(int id, pthread_t *thread, const pthread_attr_t *attr,
				void *(*start_routine) (void *), void *arg);

int wayca_managed_threadpool_create(int id, int num, pthread_t *thread[],
				    const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);

typedef unsigned long long	wayca_thread_t;
typedef unsigned long long	wayca_thread_attr_t;
/* Flags for wayca thread attributes */
#define WT_TF_PERCPU		0x1
#define WT_TF_FREE		0x2
#define WT_TF_WAYCA_MANAGEABLE	0x10000

int wayca_thread_set_attr(wayca_thread_t wthread, wayca_thread_attr_t *attr);
int wayca_thread_get_attr(wayca_thread_t wthread, wayca_thread_attr_t *attr);

int wayca_thread_create(wayca_thread_t *wthread, pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int wayca_thread_join(wayca_thread_t wthread, void **retval);

typedef unsigned long long	wayca_group_t;
typedef unsigned long long	wayca_group_attr_t;
/**
 * Flags for wayca group attributes
 * Bit[0:15]: Group thread accepted topology
 * Bit[16:19]: Group thread's binding style
 * Bit[20:32]: Group thread's relationship
 */
#define WT_GF_CPU	1		/* Each thread accepts per-CPU affinity */
#define WT_GF_CCL	4		/* Each thread accepts per-CCL affinity */
#define WT_GF_NUMA	32		/* Each thread accepts per-NUMA affinity */
#define WT_GF_PACKAGE	64		/* Each thread accepts per-Package affinity */
#define WT_GF_ALL	1024		/* Each thread doesn't have an affinity hint */
#define WT_GF_PERCPU	0x10000		/* Each thread will bind to the cpu */
// #define WT_GF_FREE	0x20000		/* Each thread will schedule in the affinity range */
#define WT_GF_COMPACT	0x100000	/* The threads in this group will be compact */
// #define WT_GF_SPAN	0x200000	/* The threads in this group will span */
#define WT_GF_FIXED	0x400000	/* The size of the group is Fixed. */

int wayca_thread_group_set_attr(wayca_group_t group, wayca_group_attr_t *attr);
int wayca_thread_group_get_attr(wayca_group_t group, wayca_group_attr_t *attr);

int wayca_thread_group_create(wayca_group_t* group);
int wayca_thread_group_destroy(wayca_group_t group);
int wayca_thread_attach_group(wayca_thread_t wthread, wayca_group_t group);
int wayca_thread_detach_group(wayca_thread_t wthread, wayca_group_t group);
int wayca_group_attach_group(wayca_group_t group, wayca_group_t father);
int wayca_group_detach_group(wayca_group_t group, wayca_group_t father);

/* For debug purpose */
int wayca_thread_get_cpuset(wayca_thread_t wthread, cpu_set_t *cpuset);
int wayca_group_get_cpuset(wayca_group_t group, cpu_set_t *cpuset);

#endif

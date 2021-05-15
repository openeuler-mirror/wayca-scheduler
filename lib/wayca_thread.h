#ifndef _WAYCA_THREAD_H
#define _WAYCA_THREAD_H

#define _GNU_SOURCE
#include <sched.h>
#include <syscall.h>
#include <wayca-scheduler.h>
#include "common.h"

#define thread_sched_setaffinity(pid, size, cpuset) \
  syscall(__NR_sched_setaffinity, (pid_t)pid, (size_t)size, (void *)cpuset)
#define thread_sched_getaffinity(pid, size, cpuset) \
  syscall(__NR_sched_getaffinity, (pid_t)pid, (size_t)size, (void *)cpuset)
#define thread_sched_gettid(void)	\
  syscall(__NR_gettid)

/* CPU set of all the cpus in the system */
extern cpu_set_t total_cpu_set;

struct wayca_thread {
	/* Wayca thread id which is identity to this thread */
	wayca_thread_t id;
	/* pid_t of this wayca thread */
	pid_t pid;
	/* Wayca thread attribute */
	wayca_thread_attr_t attribute;
	cpu_set_t cur_set;
	cpu_set_t allowed_set;
	/* Siblings of this wayca thread in the same group, NULL terminated */
	struct wayca_thread *siblings;
	/* Wayca group this thread directly belongs to */
	struct wayca_group *group;

	/* Internal pthread_t for this thread */
	pthread_t thread;
	/* The routine this thread going to perform */
	void *(*start_routine)(void *);
	/* The args for the routine */
	void *arg;
};

/**
 * TBD:
 * 	The group may add/delete threads simutaneously,
 * 	so a lock for the group is necessary to avoid race
 * 	conditions.
 */
struct wayca_group {
	/* Wayca group id which is identity to this group */
	wayca_group_t id;
	/* The threads list in this group */
	struct wayca_thread *threads;
	/* The number of the threads in this group */
	int nr_threads;
	/* The sibling groups, NULL terminated */
	struct wayca_group *siblings;
	/* The father of this group, NULL means the toppest level */
	struct wayca_group *father;
	/* The groups in this group */
	struct wayca_group *groups;
	/* The number of the groups in this group */
	int nr_groups;
	/**
	 * The cpuset of which has thread scheduled on.
	 * The set bit means it's occupied.
	 */
	cpu_set_t used;
	/**
	 * The cpuset this group owns.
	 * The set bit means it canbe used by this group.
	 */
	cpu_set_t total;
	/* The attribute specify the arrangement strategy of this group */
	wayca_group_attr_t attribute;

	/* Stride for arranging the threads */
	int stride;
	int nr_cpus_per_topo;
	/* A hint to indicates where to request cpus, -1 means no hint */
	int topo_hint;
	/* Roll over cnts */
	int roll_over_cnts;
};

#define group_for_each_threads(thread, group)	\
	for (thread = group->threads; thread != NULL; thread = thread->siblings)

/* Do the initialization work for a new create group */
int wayca_group_init(struct wayca_group *group);
/* Arrange the resource of the group according to the attribute */
int wayca_group_arrange(struct wayca_group *group);
/* Add the thread to the group, arrange the resource for it */
int wayca_group_add_thread(struct wayca_group *group, struct wayca_thread *thread);
/* Delete one thread from the group. */
int wayca_group_delete_thread(struct wayca_group *group, struct wayca_thread *thread);
/* Rearrange the resource assigned to the thread as the attribute of thread has been changed */
int wayca_group_rearrange_thread(struct wayca_group *group, struct wayca_thread *thread);
/* Rearrange all the group threads' resources as the attribute of the group has been changed */
int wayca_group_rearrange_group(struct wayca_group *group);
int wayca_group_add_group(struct wayca_group *group, struct wayca_group *father);
int wayca_group_delete_group(struct wayca_group *group, struct wayca_group *father);

static inline int cpuset_find_first_unset(cpu_set_t *cpusetp)
{
	int pos = 0;

	while (pos < cores_in_total() && CPU_ISSET(pos, cpusetp))
		pos++;

	return pos >= cores_in_total() ? -1 : pos;
}

static inline int cpuset_find_first_set(cpu_set_t *cpusetp)
{
	int pos = 0;

	while (pos < cores_in_total() && !CPU_ISSET(pos, cpusetp))
		pos++;

	return pos >= cores_in_total() ? -1 : pos;
}

static inline int cpuset_find_last_set(cpu_set_t *cpusetp)
{
	int pos = cores_in_total() - 1;

	while (pos >= 0 && !CPU_ISSET(pos, cpusetp))
		pos--;

	return pos >= 0 ? pos : -1;
}

#endif	/* _WAYCA_THREAD_H */

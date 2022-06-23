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

#ifndef _WAYCA_THREAD_H
#define _WAYCA_THREAD_H

#define _GNU_SOURCE
#include <sched.h>
#include <syscall.h>
#include <wayca-scheduler.h>
#include "common.h"
#include "bitmap.h"

static inline int thread_sched_setaffinity(pid_t pid, size_t cpusetsize,
					   const cpu_set_t *cpuset)
{
	int ret;

	ret = syscall(__NR_sched_setaffinity, pid, cpusetsize, cpuset);
	return ret < 0 ? -errno : ret;
}

static inline int thread_sched_getaffinity(pid_t pid, size_t cpusetsize,
					   cpu_set_t cpuset)
{
	int ret;

	ret = syscall(__NR_sched_getaffinity, pid, cpusetsize, cpuset);
	return ret < 0 ? -errno : ret;
}

static inline pid_t thread_sched_gettid(void)
{
	int ret;

	ret = syscall(__NR_gettid);
	return ret < 0 ? -errno : ret;
}

/* CPU set of all the cpus in the system */
extern cpu_set_t total_cpu_set;
/* Load Array of each cpu, length is cores_in_total() */
extern long long *wayca_cpu_loads;
extern pthread_mutex_t wayca_cpu_loads_mutex;

struct wayca_thread {
	/* Wayca thread id which is identity to this thread */
	wayca_sc_thread_t id;
	/* pid_t of this wayca thread */
	pid_t pid;
	/* Wayca thread attribute */
	wayca_sc_thread_attr_t attribute;
	size_t target_pos;
	cpu_set_t cur_set;
	cpu_set_t allowed_set;
	/* Siblings of this wayca thread in the same group, NULL terminated */
	struct wayca_thread *siblings;
	/* Wayca group this thread directly belongs to */
	struct wayca_sc_group *group;

	/*
	 * Following fields will be meaningful only if the thread is
	 * created by us, rather than attached by an existed thread
	 * or process.
	 */

	/* Internal pthread_t for this thread */
	pthread_t thread;
	/* The routine this thread going to perform */
	void *(*start_routine)(void *);
	/* The args for the routine */
	void *arg;
	/* Is the routine started ? */
	bool start;
};

struct wayca_sc_group {
	/* Wayca group id which is identity to this group */
	wayca_sc_group_t id;
	/* The threads list in this group */
	struct wayca_thread *threads;
	/* The number of the threads in this group */
	int nr_threads;
	/* The sibling groups, NULL terminated */
	struct wayca_sc_group *siblings;
	/* The father of this group, NULL means the toppest level */
	struct wayca_sc_group *father;
	/* The groups in this group */
	struct wayca_sc_group *groups;
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
	wayca_sc_group_attr_t attribute;
	/* The mutex to protect this data structure */
	pthread_mutex_t mutex;

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

#define group_for_each_groups(group, father)	\
	for (group = father->groups; group != NULL; group = group->siblings)

/* Do the initialization work for a new create group */
int wayca_group_init(struct wayca_sc_group *group);

/* Add the thread to the group, arrange the resource for it */
int wayca_group_add_thread(struct wayca_sc_group *group, struct wayca_thread *thread);

/* Delete one thread from the group. */
int wayca_group_delete_thread(struct wayca_sc_group *group, struct wayca_thread *thread);

/* Rearrange the resource assigned to the thread as the attribute of thread has been changed */
int wayca_group_rearrange_thread(struct wayca_sc_group *group, struct wayca_thread *thread);

/* Rearrange all the group threads' resources as the attribute of the group has been changed */
int wayca_group_rearrange_group(struct wayca_sc_group *group);

int wayca_group_add_group(struct wayca_sc_group *group, struct wayca_sc_group *father);

int wayca_group_delete_group(struct wayca_sc_group *group, struct wayca_sc_group *father);

void wayca_thread_update_load(struct wayca_thread *thread, bool add);

bool is_thread_in_group(struct wayca_sc_group *group, struct wayca_thread *thread);

bool is_group_in_father(struct wayca_sc_group *group, struct wayca_sc_group *father);

struct wayca_threadpool_task {
	/* The wayca threadpool this task belongs to */
	struct wayca_threadpool *pool;
	/* The task function */
	wayca_sc_threadpool_task_func task;
	/* The argument of the task function */
	void *arg;
	/* Previous and next task in the queue of the threadpool */
	struct wayca_threadpool_task *next, *prev;
};

struct wayca_threadpool {
	/* The taskpool id */
	wayca_sc_threadpool_t id;
	/* The wayca thread list of this threadpool */
	struct wayca_thread **workers;
	/* Total number of worker threads available in this threadpool */
	size_t total_worker_num;
	/* The number of idle workers in this threadpool */
	size_t idle_num;
	/* The head task on the queue waiting to run */
	struct wayca_threadpool_task *task_head;
	/* The number of the tasks in the queue */
	size_t task_num;
	/* The wayca sc group that the threads in this threadpool belongs to */
	struct wayca_sc_group *group;
	/* The mutex to protect this structure */
	pthread_mutex_t mutex;
	/* Conditonal variable to wakeup threads */
	pthread_cond_t cond;
	/* True to Notify the workers to stop */
	bool stop;
};

static inline bool threadpool_task_is_empty(struct wayca_threadpool *pool)
{
	return pool->task_head == NULL;
}

#endif	/* _WAYCA_THREAD_H */

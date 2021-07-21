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

#define _GNU_SOURCE
#include <sched.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <wayca-scheduler.h>
#include "wayca_thread.h"

WAYCA_SC_INIT_PRIO(wayca_thread_init, THREAD);
WAYCA_SC_FINI_PRIO(wayca_thread_exit, THREAD);

static inline void set_cpu_mask(int cpu, cpu_set_t * mask)
{
	CPU_ZERO(mask);
	CPU_SET(cpu, mask);
}

static inline void set_ccl_cpumask(int cpu, cpu_set_t * mask)
{
	CPU_ZERO(mask);
	for (int i = 0; i < wayca_sc_cpus_in_ccl(); i++)
		CPU_SET(cpu + i, mask);
}

static inline void set_node_cpumask(int node, cpu_set_t * mask)
{
	int cr_in_node = wayca_sc_cpus_in_node();

	CPU_ZERO(mask);
	for (int i = 0; i < cr_in_node; i++)
		CPU_SET(node * cr_in_node + i, mask);
}

static inline void set_package_cpumask(int node, cpu_set_t * mask)
{
	int nr_in_pack = wayca_sc_nodes_in_package();
	int cr_in_node = wayca_sc_cpus_in_node();
	int cr_in_pack = wayca_sc_cpus_in_package();
	node = node / nr_in_pack * nr_in_pack;

	CPU_ZERO(mask);
	for (int i = 0; i < cr_in_pack; i++)
		CPU_SET(node * cr_in_node + i, mask);
}

static inline void set_all_cpumask(cpu_set_t * mask)
{
	int cr_in_total = wayca_sc_cpus_in_total();

	CPU_ZERO(mask);
	for (int i = 0; i < cr_in_total; i++)
		CPU_SET(i, mask);
}

int list_to_mask(char *s, cpu_set_t * mask)
{
	int cr_in_total = wayca_sc_cpus_in_total();

	CPU_ZERO(mask);
	while(1) {
		unsigned long start, end;
		unsigned long stride = 1;
		errno = 0; /* strtoul can set this to non-zero */

		start = end = strtoul(s, &s, 10);
		if (*s == '-') {
			s++;
			end = strtoul(s, &s, 10);
			if (*s == ':') {
				s++;
				stride = strtoul(s, &s, 10);
			}
		}
		if ((*s != ',' && *s != '\0') ||
		    stride >= cr_in_total/2 ||
		    errno || end >= cr_in_total) {
			perror("bad affinity");
			return -1;
		}

		for (int i = start; i <= end; i += stride)
			CPU_SET(i, mask);
		if (*s == '\0')
			break;
		s++;
	}

	return 0;
}

/* bind a thread to a specified CPU */
int thread_bind_cpu(pid_t pid, int cpu)
{
	cpu_set_t mask;
	set_cpu_mask(cpu, &mask);
	return thread_sched_setaffinity(pid, sizeof(mask), &mask);
}

/* bind a thread to a CCL which starts from a specified CPU */
int thread_bind_ccl(pid_t pid, int cpu)
{
	cpu_set_t mask;
	set_ccl_cpumask(cpu, &mask);
	return thread_sched_setaffinity(pid, sizeof(mask), &mask);
}

/* bind a thread to a numa node */
int thread_bind_node(pid_t pid, int node)
{
	cpu_set_t mask;
	set_node_cpumask(node, &mask);
	return thread_sched_setaffinity(pid, sizeof(mask), &mask);
}

/* bind a thread to a package which might include multiple numa nodes */
int thread_bind_package(pid_t pid, int node)
{
	cpu_set_t mask;
	set_package_cpumask(node, &mask);
	return thread_sched_setaffinity(pid, sizeof(mask), &mask);
}

/* unbind a thread, aka. bind to all CPUs */
int thread_unbind(pid_t pid)
{
	cpu_set_t mask;
	set_all_cpumask(&mask);
	return thread_sched_setaffinity(pid, sizeof(mask), &mask);
}

/* bind a thread to cpulist defined by a string like "0-3,5" */
int thread_bind_cpulist(pid_t pid, char *s)
{
	cpu_set_t mask;
	if (list_to_mask(s, &mask))
		return -1;
	return thread_sched_setaffinity(pid, sizeof(mask), &mask);
}

int process_bind_cpulist(pid_t pid, char *s)
{
	cpu_set_t mask;
	if (list_to_mask(s, &mask))
		return -1;
	return thread_sched_setaffinity(pid, sizeof(mask), &mask);
}

/*
 * kernel only provides sched_setaffinity to set thread affinity
 * this API can set affinity for the whole process
 */
int process_sched_setaffinity(pid_t pid, int size, cpu_set_t * mask)
{
	char buf[PATH_MAX];
	struct dirent *de;
	DIR *d;
	int ret;

	sprintf(buf, "/proc/%d/task/", pid);
	d = opendir(buf);
	if (!d)
		return -1;

	while ((de = readdir(d)) != NULL) {
		pid_t tid;

		if (strcmp(de->d_name, ".") == 0
		    || strcmp(de->d_name, "..") == 0)
			continue;
		tid = strtoul(de->d_name, NULL, 10);
		ret = thread_sched_setaffinity(tid, size, mask);
		if (ret < 0)
			break;
	}
	closedir(d);

	return ret;
}

/*
 * bind all threads in a process to a specified CPU
 */
int process_bind_cpu(pid_t pid, int cpu)
{
	cpu_set_t mask;
	set_cpu_mask(cpu, &mask);
	return process_sched_setaffinity(pid, sizeof(mask), &mask);
}

/*
 * bind all threads in a process to a CCL which starts from a specified CPU
 */
int process_bind_ccl(pid_t pid, int cpu)
{
	cpu_set_t mask;
	set_ccl_cpumask(cpu, &mask);
	return process_sched_setaffinity(pid, sizeof(mask), &mask);
}

/*
 * bind all threads in a process to a numa node
 */
int process_bind_node(pid_t pid, int node)
{
	cpu_set_t mask;
	set_node_cpumask(node, &mask);
	return process_sched_setaffinity(pid, sizeof(mask), &mask);
}

/* bind all threads in a process to a package which might include multiple numa nodes */
int process_bind_package(pid_t pid, int node)
{
	cpu_set_t mask;
	set_package_cpumask(node, &mask);
	return process_sched_setaffinity(pid, sizeof(mask), &mask);
}

/* unbind all threads in one process, aka. bind to all CPUs */
int process_unbind(pid_t pid)
{
	cpu_set_t mask;
	set_all_cpumask(&mask);
	return process_sched_setaffinity(pid, sizeof(mask), &mask);
}

/* bind all threads in one process to cpulist defined by a string like "0-3,5" */
int process_bind_cpumask(pid_t pid, cpu_set_t *cpumask, size_t maxCpus)
{
	return process_sched_setaffinity(pid, maxCpus, cpumask);
}

int thread_bind_cpumask(pid_t pid, cpu_set_t *cpumask, size_t maxCpus)
{
	return process_sched_setaffinity(pid, maxCpus, cpumask);
}

char *wayca_scheduler_socket_path = "/etc/wayca-scheduler/wayca.socket";

#define DEFAULT_WAYCA_THREAD_NUM	65536
static struct wayca_thread **wayca_threads_array;
static pthread_mutex_t wayca_threads_array_mutex;
static int wayca_threads_array_size;

#define DEFAULT_WAYCA_GROUP_NUM		256
static struct wayca_sc_group **wayca_groups_array;
static pthread_mutex_t wayca_groups_array_mutex;
static int wayca_groups_array_size;

#define DEFAULT_WAYCA_THREADPOOL_NUM	256
static struct wayca_threadpool	**wayca_threadpools_array;
static pthread_mutex_t wayca_threadpools_array_mutex;
static int wayca_threadpools_num;

cpu_set_t total_cpu_set;

long long *wayca_cpu_loads;
pthread_mutex_t wayca_cpu_loads_mutex;

static void wayca_thread_init(void)
{
	size_t num, total_cpu_cnt;
	char *p;

	CPU_ZERO(&total_cpu_set);
	total_cpu_cnt = wayca_sc_cpus_in_total();
	for (int cpu = 0; cpu < total_cpu_cnt; cpu++)
		CPU_SET(cpu, &total_cpu_set);

	wayca_cpu_loads = malloc(total_cpu_cnt * sizeof(long long));
	memset(wayca_cpu_loads, 0, sizeof(long long) * total_cpu_cnt);
	pthread_mutex_init(&wayca_cpu_loads_mutex, NULL);

	p = secure_getenv("WAYCA_THREADS_NUMBER");
	if (p)
		num = strtoull(p, NULL, 10);

	if (!p || num == ULLONG_MAX)
		num = DEFAULT_WAYCA_THREAD_NUM;

	wayca_threads_array = malloc(num * sizeof(struct wayca_thread *));
	memset(wayca_threads_array, 0, num * sizeof(struct wayca_thread *));
	wayca_threads_array_size = num;
	pthread_mutex_init(&wayca_threads_array_mutex, NULL);

	p = secure_getenv("WAYCA_GROUP_NUMBERS");
	if (p)
		num = atoi(p);
	else
		num = DEFAULT_WAYCA_GROUP_NUM;

	wayca_groups_array = malloc(num * sizeof(struct wayca_sc_group *));
	memset(wayca_groups_array, 0, num * sizeof(struct wayca_sc_group *));
	wayca_groups_array_size = num;
	pthread_mutex_init(&wayca_groups_array_mutex, NULL);

	p = secure_getenv("WAYCA_THREADPOOL_NUMBERS");
	if (p)
		num = atoi(p);
	else
		num = DEFAULT_WAYCA_THREADPOOL_NUM;

	wayca_threadpools_array = malloc(num * sizeof(struct wayca_threadpool));
	memset(wayca_threadpools_array, 0, num * sizeof(struct wayca_threadpool));
	wayca_threadpools_num = num;
	pthread_mutex_init(&wayca_threadpools_array_mutex, NULL);
}

static void wayca_thread_exit(void)
{
	free(wayca_cpu_loads);
	pthread_mutex_destroy(&wayca_cpu_loads_mutex);

	free(wayca_threads_array);
	pthread_mutex_destroy(&wayca_threads_array_mutex);

	free(wayca_groups_array);
	pthread_mutex_destroy(&wayca_groups_array_mutex);

	free(wayca_threadpools_array);
	pthread_mutex_destroy(&wayca_threadpools_array_mutex);
}

/**
 * The caller should have hold the @wayca_threads_array_mutex lock.
 */
int find_free_thread_id_locked()
{
	for (wayca_sc_thread_t i = 0; i < wayca_threads_array_size; i++)
		if (!wayca_threads_array[i])
			return i;

	return -1;
}

/**
 * The caller should have hold the @wayca_groups_array_mutex lock.
 */
int find_free_group_id_locked()
{
	for (wayca_sc_group_t i = 0; i < wayca_groups_array_size; i++)
		if (!wayca_groups_array[i])
			return i;

	return -1;
}

/**
 * The caller should have hold the @wayca_threadpools_array_mutex lock.
 */
int find_free_threadpool_id_locked()
{
	for (wayca_sc_threadpool_t i = 0; i < wayca_threadpools_num; i++)
		if (!wayca_threadpools_array[i])
			return i;

	return -1;
}

bool is_thread_id_valid(wayca_sc_thread_t id)
{
	bool valid;

	pthread_mutex_lock(&wayca_threads_array_mutex);
	valid = wayca_threads_array[id] != NULL;
	pthread_mutex_unlock(&wayca_threads_array_mutex);

	return valid;
}

bool is_group_id_valid(wayca_sc_group_t id)
{
	bool valid;

	pthread_mutex_lock(&wayca_groups_array_mutex);
	valid = wayca_groups_array[id] != NULL;
	pthread_mutex_unlock(&wayca_groups_array_mutex);

	return valid;
}

bool is_threadpool_id_valid(wayca_sc_threadpool_t id)
{
	bool valid;

	if (id < 0 || id >= wayca_threadpools_num)
		return false;

	pthread_mutex_lock(&wayca_threadpools_array_mutex);
	valid = wayca_threadpools_array[id] != NULL;
	pthread_mutex_unlock(&wayca_threadpools_array_mutex);

	return valid;
}

struct wayca_thread *id_to_wayca_thread(wayca_sc_thread_t id)
{
	return wayca_threads_array[id];
}

struct wayca_sc_group *id_to_wayca_group(wayca_sc_group_t id)
{
	return wayca_groups_array[id];
}

struct wayca_threadpool *id_to_wayca_threadpool(wayca_sc_threadpool_t id)
{
	return wayca_threadpools_array[id];
}

void *wayca_thread_start_routine(void *private)
{
	struct wayca_thread *thread = private;
	cpu_set_t cpuset;

	thread->pid = thread_sched_gettid();

	sched_getaffinity(thread->pid, sizeof(cpu_set_t), &cpuset);
	CPU_ZERO(&thread->cur_set);
	CPU_ZERO(&thread->allowed_set);

	CPU_OR(&thread->cur_set, &thread->cur_set, &cpuset);
	CPU_OR(&thread->allowed_set, &thread->allowed_set, &cpuset);

	wayca_thread_update_load(thread, true);

	thread->start = true;
	return thread->start_routine(thread->arg);
}

int wayca_sc_thread_set_attr(wayca_sc_thread_t wthread, wayca_sc_thread_attr_t *attr)
{
	struct wayca_thread *wt_p;

	if (!is_thread_id_valid(wthread))
		return -1;

	wt_p = id_to_wayca_thread(wthread);
	wt_p->attribute = *attr;

	if (wt_p->group)
		wayca_group_rearrange_thread(wt_p->group, wt_p);

	return thread_sched_setaffinity(wt_p->pid, sizeof(cpu_set_t), &wt_p->cur_set);
}

int wayca_sc_thread_get_attr(wayca_sc_thread_t wthread, wayca_sc_thread_attr_t *attr)
{
	struct wayca_thread *wt_p;

	if (!is_thread_id_valid(wthread))
		return -1;

	wt_p = id_to_wayca_thread(wthread);
	*attr = wt_p->attribute;

	return 0;
}

struct wayca_thread *wayca_thread_alloc()
{
	wayca_sc_thread_t id;

	pthread_mutex_lock(&wayca_threads_array_mutex);
	id = find_free_thread_id_locked();
	if (id == -1)
		goto err;

	wayca_threads_array[id] = malloc(sizeof(struct wayca_thread));
	if (!wayca_threads_array[id])
		goto err;

	pthread_mutex_unlock(&wayca_threads_array_mutex);

	memset(wayca_threads_array[id], 0, sizeof(struct wayca_thread));
	wayca_threads_array[id]->id = id;

	return wayca_threads_array[id];
err:
	pthread_mutex_unlock(&wayca_threads_array_mutex);
	return NULL;
}

void wayca_thread_free(struct wayca_thread *thread)
{
	pthread_mutex_lock(&wayca_threads_array_mutex);
	wayca_threads_array[thread->id] = NULL;
	free(thread);
	pthread_mutex_unlock(&wayca_threads_array_mutex);
}

int wayca_sc_thread_create(wayca_sc_thread_t *wthread, pthread_attr_t *attr,
			void *(*start_routine)(void *), void *arg)
{
	struct wayca_thread *wt_p;
	pthread_t *pthread_ptr;
	int retval;

	wt_p = wayca_thread_alloc();
	if (!wt_p)
		return -1;

	wt_p->siblings = NULL;
	wt_p->group = NULL;
	wt_p->start_routine = start_routine;
	wt_p->arg = arg;
	wt_p->start = false;
	pthread_ptr = &wt_p->thread;

	retval = pthread_create(pthread_ptr, attr,
				wayca_thread_start_routine, wt_p);
	if (retval < 0) {
		wayca_thread_free(wt_p);
		return retval;
	}

	while (!wt_p->start)
		;

	*wthread = wt_p->id;
	return 0;
}

int wayca_sc_thread_join(wayca_sc_thread_t id, void **retval)
{
	struct wayca_thread *thread;
	int ret;

	if (!is_thread_id_valid(id))
		return -1; /* Invalid id */

	thread = id_to_wayca_thread(id);

	ret = pthread_join(thread->thread, retval);

	if (thread->group)
		wayca_sc_thread_detach_group(id, thread->group->id);

	wayca_thread_update_load(thread, false);

	wayca_thread_free(thread);

	return ret;
}

int wayca_sc_group_set_attr(wayca_sc_group_t group, wayca_sc_group_attr_t *attr)
{
	struct wayca_sc_group *wg_p;
	int ret;

	if (!is_group_id_valid(group))
		return -1;

	wg_p = id_to_wayca_group(group);

	pthread_mutex_lock(&wg_p->mutex);
	wg_p->attribute = *attr;

	ret = wayca_group_rearrange_group(wg_p);

	pthread_mutex_unlock(&wg_p->mutex);
	return ret;
}

int wayca_sc_group_get_attr(wayca_sc_group_t group, wayca_sc_group_attr_t *attr)
{
	struct wayca_sc_group *wg_p;

	if (!is_group_id_valid(group))
		return -1;

	wg_p = id_to_wayca_group(group);

	pthread_mutex_lock(&wg_p->mutex);
	*attr = wg_p->attribute;
	pthread_mutex_unlock(&wg_p->mutex);

	return 0;
}

struct wayca_sc_group *wayca_group_alloc()
{
	wayca_sc_group_t id;

	pthread_mutex_lock(&wayca_groups_array_mutex);
	id = find_free_group_id_locked();
	if (id == -1)
		goto err;

	wayca_groups_array[id] = malloc(sizeof(struct wayca_sc_group));
	if (!wayca_groups_array[id])
		goto err;

	pthread_mutex_unlock(&wayca_groups_array_mutex);

	memset(wayca_groups_array[id], 0, sizeof(struct wayca_sc_group));
	wayca_groups_array[id]->id = id;

	return wayca_groups_array[id];
err:
	pthread_mutex_unlock(&wayca_groups_array_mutex);
	return NULL;
}

void wayca_group_free(struct wayca_sc_group *group)
{
	pthread_mutex_lock(&wayca_groups_array_mutex);
	wayca_groups_array[group->id] = NULL;
	free(group);
	pthread_mutex_unlock(&wayca_groups_array_mutex);
}

int wayca_sc_group_create(wayca_sc_group_t *group)
{
	struct wayca_sc_group *wg_p;

	wg_p = wayca_group_alloc();
	if (!wg_p)
		return -1;

	if (wayca_group_init(wg_p)) {
		wayca_group_free(wg_p);
		return -1;
	}

	*group = wg_p->id;
	return 0;
}

int wayca_sc_group_destroy(wayca_sc_group_t group)
{
	struct wayca_sc_group *wg_p;

	if (!is_group_id_valid(group))
		return -1;

	wg_p = id_to_wayca_group(group);

	/* We still have threads in this group, stop destroy */
	if (wg_p->nr_threads)
		return -1;

	wayca_group_free(wg_p);

	return 0;
}

int wayca_sc_thread_attach_group(wayca_sc_thread_t wthread, wayca_sc_group_t group)
{
	struct wayca_thread *wt_p;
	struct wayca_sc_group *wg_p;
	int ret;

	if (!is_thread_id_valid(wthread) || !is_group_id_valid(group))
		return -1;

	wt_p = id_to_wayca_thread(wthread);
	wg_p = id_to_wayca_group(group);

	wayca_thread_update_load(wt_p, false);

	pthread_mutex_lock(&wg_p->mutex);
	if (wayca_group_add_thread(wg_p, wt_p))
		ret = -1;
	else
		ret = wayca_group_rearrange_thread(wg_p, wt_p);

	pthread_mutex_unlock(&wg_p->mutex);
	return ret;
}

int wayca_sc_thread_detach_group(wayca_sc_thread_t wthread, wayca_sc_group_t group)
{
	struct wayca_thread *wt_p;
	struct wayca_sc_group *wg_p;
	int ret;

	if (!is_thread_id_valid(wthread) || !is_group_id_valid(group))
		return -1;

	wt_p = id_to_wayca_thread(wthread);
	wg_p = id_to_wayca_group(group);

	pthread_mutex_lock(&wg_p->mutex);
	ret = wayca_group_delete_thread(wg_p, wt_p);
	pthread_mutex_unlock(&wg_p->mutex);

	return ret;
}

int wayca_sc_group_attach_group(wayca_sc_group_t group, wayca_sc_group_t father)
{
	struct wayca_sc_group *wg_p, *father_p;
	int ret;

	if (!is_group_id_valid(group) || !is_group_id_valid(father))
		return -1;

	wg_p = id_to_wayca_group(group);
	father_p = id_to_wayca_group(father);

	/* If @group already has a father, should detach first before attach to a new one. */
	if (wg_p->father)
		return -1;

	pthread_mutex_lock(&wg_p->mutex);
	pthread_mutex_lock(&father_p->mutex);
	ret = wayca_group_add_group(wg_p, father_p);
	pthread_mutex_unlock(&father_p->mutex);
	pthread_mutex_unlock(&wg_p->mutex);

	return ret;
}

int wayca_sc_group_detach_group(wayca_sc_group_t group, wayca_sc_group_t father)
{
	struct wayca_sc_group *wg_p, *father_p;
	int ret;

	if (!is_group_id_valid(group) || !is_group_id_valid(father))
		return -1;

	wg_p = id_to_wayca_group(group);
	father_p = id_to_wayca_group(father);

	pthread_mutex_lock(&wg_p->mutex);
	pthread_mutex_lock(&father_p->mutex);
	ret = wayca_group_delete_group(wg_p, father_p);
	pthread_mutex_unlock(&father_p->mutex);
	pthread_mutex_unlock(&wg_p->mutex);

	return ret;
}

int wayca_sc_is_thread_in_group(wayca_sc_thread_t thread, wayca_sc_group_t group)
{
	struct wayca_sc_group *wg_p;
	struct wayca_thread *wt_p;

	wt_p = id_to_wayca_thread(thread);
	wg_p = id_to_wayca_group(group);
	return is_thread_in_group(wg_p, wt_p);
}

int wayca_sc_is_group_in_group(wayca_sc_group_t target, wayca_sc_group_t group)
{
	struct wayca_sc_group *wg_p, *father_p;

	wg_p = id_to_wayca_group(target);
	father_p = id_to_wayca_group(group);
	return is_group_in_father(wg_p, father_p);
}

static void threadpool_queue_task(struct wayca_threadpool *pool,
			   struct wayca_threadpool_task *task)
{
	if (threadpool_task_is_empty(pool)) {
		WAYCA_SC_ASSERT(!pool->task_num);
		pool->task_head = task;
		pool->task_head->next = task;
		pool->task_head->prev = task;
	} else {
		struct wayca_threadpool_task *prev, *next;
		WAYCA_SC_ASSERT(pool->task_num);
		prev = pool->task_head->prev;
		next = pool->task_head;

		next->prev = task;
		task->next = next;
		task->prev = prev;
		prev->next = task;
	}

	pool->task_num++;
}

static struct wayca_threadpool_task *threadpool_dequeue_task(struct wayca_threadpool *pool)
{
	struct wayca_threadpool_task *queue, *next, *prev;

	if (threadpool_task_is_empty(pool))
		return NULL;

	queue = pool->task_head;
	WAYCA_SC_ASSERT(queue);

	/* If there is only one task in the queue */
	if (queue->next == queue) {
		WAYCA_SC_ASSERT(pool->task_num == 1);
		pool->task_head = NULL;
	} else {
		WAYCA_SC_ASSERT(pool->task_num >= 2);
		next = queue->next;
		prev = queue->prev;

		next->prev = prev;
		prev->next = next;
		pool->task_head = next;

	}

	pool->task_num--;
	return queue;
}

static void *wayca_threadpool_worker_func(void *priv)
{
	struct wayca_threadpool *pool = priv;
	struct wayca_threadpool_task *task;

	while (true) {
		pthread_mutex_lock(&pool->mutex);
		if (pool->stop) {
			pthread_mutex_unlock(&pool->mutex);
			break;
		}

		if (!pool->task_num) {
			pthread_cond_wait(&pool->cond, &pool->mutex);
			pthread_mutex_unlock(&pool->mutex);
			continue;
		}

		WAYCA_SC_ASSERT(pool->task_head);
		task = threadpool_dequeue_task(pool);
		pool->idle_num--;

		pthread_mutex_unlock(&pool->mutex);

		task->task(task->arg);
		free(task);

		pthread_mutex_lock(&pool->mutex);
		pool->idle_num++;
		pthread_mutex_unlock(&pool->mutex);
	}

	return NULL;
}

static struct wayca_threadpool *wayca_threadpool_alloc(size_t thread_num)
{
	wayca_sc_threadpool_t id;

	pthread_mutex_lock(&wayca_threadpools_array_mutex);
	id = find_free_threadpool_id_locked();
	if (id < 0)
		goto err;

	wayca_threadpools_array[id] = malloc(sizeof(struct wayca_threadpool));
	if (!wayca_threadpools_array[id])
		goto err;
	memset(wayca_threadpools_array[id], 0, sizeof(struct wayca_threadpool));

	wayca_threadpools_array[id]->workers = malloc(thread_num * sizeof(struct wayca_threads *));
	if (!wayca_threadpools_array[id]) {
		free(wayca_threadpools_array[id]);
		wayca_threadpools_array[id] = NULL;
		goto err;
	}

	pthread_mutex_unlock(&wayca_threadpools_array_mutex);
	wayca_threadpools_array[id]->id = id;

	return wayca_threadpools_array[id];
err:
	pthread_mutex_unlock(&wayca_threadpools_array_mutex);
	return NULL;
}

static void wayca_threadpool_free(struct wayca_threadpool *pool)
{
	pthread_mutex_lock(&wayca_threadpools_array_mutex);
	wayca_threadpools_array[pool->id] = NULL;
	free(pool->workers);
	free(pool);
	pthread_mutex_unlock(&wayca_threadpools_array_mutex);
}

static int wayca_threadpool_init(struct wayca_threadpool *pool, size_t num)
{
	wayca_sc_group_attr_t group_attr;
	struct wayca_thread *thread;
	wayca_sc_group_t wgroup;
	wayca_sc_thread_t wthread;
	int ret, worker_num;

	ret = wayca_sc_group_create(&wgroup);
	if (ret)
		return ret;

	group_attr = WT_GF_CPU | WT_GF_COMPACT | WT_GF_PERCPU;
	ret = wayca_sc_group_set_attr(wgroup, &group_attr);
	if (ret) {
		wayca_sc_group_destroy(wgroup);
		return ret;
	}

	pool->group = id_to_wayca_group(wgroup);
	pool->stop = false;
	pthread_mutex_init(&pool->mutex, NULL);
	pthread_cond_init(&pool->cond, NULL);

	for (worker_num = 0; worker_num < num; worker_num++) {
		ret = wayca_sc_thread_create(&wthread, NULL, wayca_threadpool_worker_func, pool);
		if (ret)
			break;

		thread = id_to_wayca_thread(wthread);
		pool->workers[worker_num] = thread;

		ret = wayca_sc_thread_attach_group(wthread, wgroup);
		if (ret) {
			pthread_kill(thread->thread, SIGKILL);
			wayca_sc_thread_join(wthread, NULL);
			break;
		}
	}

	pool->total_worker_num = worker_num;
	pool->idle_num = worker_num;
	pool->task_head = NULL;
	pool->task_num = 0;

	return 0;
}

int wayca_sc_threadpool_create(wayca_sc_threadpool_t *threadpool, size_t num)
{
	struct wayca_threadpool *pool;

	if (num <= 0)
		return -EINVAL;

	pool = wayca_threadpool_alloc(num);
	if (!pool)
		return -ENOMEM;

	if (wayca_threadpool_init(pool, num)) {
		wayca_threadpool_free(pool);
		return -ENOMEM;
	}

	*threadpool = pool->id;
	return pool->total_worker_num;
}

int wayca_sc_threadpool_destroy(wayca_sc_threadpool_t threadpool, int force)
{
	struct wayca_threadpool *pool;
	struct wayca_threadpool_task *task;

	if (!is_threadpool_id_valid(threadpool))
		return -EINVAL;

	pool = id_to_wayca_threadpool(threadpool);

	/* Wait for the finish of current running tasks */
	pthread_mutex_lock(&pool->mutex);
	pool->stop = true;
	pthread_cond_broadcast(&pool->cond);
	pthread_mutex_unlock(&pool->mutex);


	for (int worker = 0; worker < pool->total_worker_num; worker++) {
		if (force)
			kill(pool->workers[worker]->pid, SIGKILL);
		wayca_sc_thread_join(pool->workers[worker]->id, NULL);
	}

	pthread_mutex_lock(&pool->mutex);
	wayca_sc_group_destroy(pool->group->id);

	while (!threadpool_task_is_empty(pool)) {
		task = threadpool_dequeue_task(pool);
		free(task);
	}

	/**
	 * We have to unlock the mutex before destroy it.
	 * As mentioned in the manual:
	 * "Attempting to destroy a locked mutex, or a mutex that
	 * another thread is attempting to lock, or a mutex that is being
	 * used in a pthread_cond_timedwait() or pthread_cond_wait() call by
	 * another thread, results in undefined behavior."
	 * ref: https://man7.org/linux/man-pages/man3/pthread_mutex_destroy.3p.html
	 * 
	 * However there is a race condition here, as we've destroy the threadpool
	 * and release the lock in the critical region, another thread may take
	 * the lock and try to queue the task or do something else.
	 */
	pthread_mutex_unlock(&pool->mutex);

	wayca_threadpool_free(pool);

	return 0;
}

int wayca_sc_threadpool_get_group(wayca_sc_threadpool_t threadpool, wayca_sc_group_t *group)
{
	struct wayca_threadpool *pool;

	if (!is_threadpool_id_valid(threadpool))
		return -EINVAL;

	if (!group)
		return -EINVAL;

	pool = id_to_wayca_threadpool(threadpool);

	pthread_mutex_lock(&pool->mutex);
	*group = pool->group->id;
	pthread_mutex_unlock(&pool->mutex);

	return 0;
}

int wayca_sc_threadpool_queue(wayca_sc_threadpool_t threadpool,
			   wayca_sc_threadpool_task_func task_func, void *arg)
{
	struct wayca_threadpool *pool;
	struct wayca_threadpool_task *task;
	int ret = 0;

	if (!is_threadpool_id_valid(threadpool))
		return -EINVAL;

	pool = id_to_wayca_threadpool(threadpool);

	task = malloc(sizeof(struct wayca_threadpool_task));
	if (!task) {
		ret = -ENOMEM;
		goto out;
	}

	task->pool = pool;
	task->task = task_func;
	task->arg = arg;

	pthread_mutex_lock(&pool->mutex);
	threadpool_queue_task(pool, task);

	if (pool->idle_num)
		pthread_cond_signal(&pool->cond);
	pthread_mutex_unlock(&pool->mutex);
out:
	return ret;
}

int wayca_sc_threadpool_thread_num(wayca_sc_threadpool_t threadpool)
{
	struct wayca_threadpool *pool;

	if (!is_threadpool_id_valid(threadpool))
		return -EINVAL;

	pool = id_to_wayca_threadpool(threadpool);
	return pool->total_worker_num;
}

int wayca_sc_threadpool_task_num(wayca_sc_threadpool_t threadpool)
{
	struct wayca_threadpool *pool;
	size_t task_num;

	if (!is_threadpool_id_valid(threadpool))
		return -EINVAL;

	pool = id_to_wayca_threadpool(threadpool);

	pthread_mutex_lock(&pool->mutex);
	task_num = pool->task_num;
	pthread_mutex_unlock(&pool->mutex);

	return task_num;
}

int wayca_sc_threadpool_running_num(wayca_sc_threadpool_t threadpool)
{
	struct wayca_threadpool *pool;
	size_t running_num;

	if (!is_threadpool_id_valid(threadpool))
		return -EINVAL;

	pool = id_to_wayca_threadpool(threadpool);
	pthread_mutex_lock(&pool->mutex);
	running_num = pool->total_worker_num - pool->idle_num;
	pthread_mutex_unlock(&pool->mutex);

	return running_num;
}


#ifdef WAYCA_SC_DEBUG
int wayca_sc_thread_get_cpuset(wayca_sc_thread_t wthread, size_t cpusetsize,
			       cpu_set_t *cpuset)
{
	struct wayca_thread *wt_p;
	size_t valid_cpu_setsize;

	if (!cpuset)
		return -EINVAL;

	if (!is_thread_id_valid(wthread))
		return -EINVAL;

	valid_cpu_setsize = CPU_ALLOC_SIZE(wayca_sc_cpus_in_total());
	if (cpusetsize < valid_cpu_setsize)
		return -EINVAL;

	wt_p = id_to_wayca_thread(wthread);

	CPU_ZERO(cpuset);
	CPU_OR(cpuset, cpuset, &wt_p->cur_set);

	return 0;
}

int wayca_sc_group_get_cpuset(wayca_sc_group_t group, size_t cpusetsize,
			      cpu_set_t *cpuset)
{
	struct wayca_sc_group *wg_p;
	size_t valid_cpu_setsize;

	if (!cpuset)
		return -EINVAL;

	if (!is_group_id_valid(group))
		return -EINVAL;

	valid_cpu_setsize = CPU_ALLOC_SIZE(wayca_sc_cpus_in_total());
	if (cpusetsize < valid_cpu_setsize)
		return -EINVAL;

	wg_p = id_to_wayca_group(group);

	CPU_ZERO(cpuset);
	CPU_OR(cpuset, cpuset, &wg_p->total);

	return 0;
}
#endif /* WAYCA_SC_DEBUG */

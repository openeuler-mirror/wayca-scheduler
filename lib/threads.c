#define _GNU_SOURCE
#include <sched.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <wayca-scheduler.h>
#include "wayca_thread.h"

static inline void set_cpu_mask(int cpu, cpu_set_t * mask)
{
	CPU_ZERO(mask);
	CPU_SET(cpu, mask);
}

static inline void set_ccl_cpumask(int cpu, cpu_set_t * mask)
{
	CPU_ZERO(mask);
	for (int i = 0; i < cores_in_ccl(); i++)
		CPU_SET(cpu + i, mask);
}

static inline void set_node_cpumask(int node, cpu_set_t * mask)
{
	int cr_in_node = cores_in_node();

	CPU_ZERO(mask);
	for (int i = 0; i < cr_in_node; i++)
		CPU_SET(node * cr_in_node + i, mask);
}

static inline void set_package_cpumask(int node, cpu_set_t * mask)
{
	int nr_in_pack = nodes_in_package();
	int cr_in_node = cores_in_node();
	int cr_in_pack = cores_in_package();
	node = node / nr_in_pack * nr_in_pack;

	CPU_ZERO(mask);
	for (int i = 0; i < cr_in_pack; i++)
		CPU_SET(node * cr_in_node + i, mask);
}

static inline void set_all_cpumask(cpu_set_t * mask)
{
	int cr_in_total = cores_in_total();

	CPU_ZERO(mask);
	for (int i = 0; i < cr_in_total; i++)
		CPU_SET(i, mask);
}

int list_to_mask(char *s, cpu_set_t * mask)
{
	int cr_in_total = cores_in_total();

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
static struct wayca_group **wayca_groups_array;
static pthread_mutex_t wayca_groups_array_mutex;
static int wayca_groups_array_size;

cpu_set_t total_cpu_set;

long long *wayca_cpu_loads;
pthread_mutex_t wayca_cpu_loads_mutex;

__attribute__((constructor)) void wayca_thread_init(void)
{
	size_t num, total_cpu_cnt;
	char *p;

	CPU_ZERO(&total_cpu_set);
	total_cpu_cnt = cores_in_total();
	for (int cpu = 0; cpu < total_cpu_cnt; cpu++)
		CPU_SET(cpu, &total_cpu_set);

	wayca_cpu_loads = malloc(total_cpu_cnt * sizeof(long long));
	memset(wayca_cpu_loads, 0, sizeof(long long) * total_cpu_cnt);
	pthread_mutex_init(&wayca_cpu_loads_mutex, NULL);

	p = secure_getenv("WAYCA_THREADS_NUMBER");
	if (p)
		num = atoi(p);
	else
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

	wayca_groups_array = malloc(num * sizeof(struct wayca_group *));
	memset(wayca_groups_array, 0, num * sizeof(struct wayca_group *));
	wayca_groups_array_size = num;
	pthread_mutex_init(&wayca_groups_array_mutex, NULL);
}

__attribute__((destructor)) void wayca_thread_exit(void)
{
	free(wayca_cpu_loads);
	pthread_mutex_destroy(&wayca_cpu_loads_mutex);

	free(wayca_threads_array);
	pthread_mutex_destroy(&wayca_threads_array_mutex);

	free(wayca_groups_array);
	pthread_mutex_destroy(&wayca_groups_array_mutex);
}

/**
 * The caller should have hold the @wayca_threads_array_mutex lock.
 */
int find_free_thread_id_locked()
{
	for (wayca_thread_t i = 0; i < wayca_threads_array_size; i++)
		if (!wayca_threads_array[i])
			return i;

	return -1;
}

/**
 * The caller should have hold the @wayca_groups_array_mutex lock.
 */
int find_free_group_id_locked()
{
	for (wayca_group_t i = 0; i < wayca_groups_array_size; i++)
		if (!wayca_groups_array[i])
			return i;

	return -1;
}

bool is_thread_id_valid(wayca_thread_t id)
{
	return wayca_threads_array[id] != NULL;
}

bool is_group_id_valid(wayca_group_t id)
{
	return wayca_groups_array[id] != NULL;
}

struct wayca_thread *id_to_wayca_thread(wayca_thread_t id)
{
	return wayca_threads_array[id];
}

struct wayca_group *id_to_wayca_group(wayca_group_t id)
{
	return wayca_groups_array[id];
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

int wayca_thread_set_attr(wayca_thread_t wthread, wayca_thread_attr_t *attr)
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

int wayca_thread_get_attr(wayca_thread_t wthread, wayca_thread_attr_t *attr)
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
	wayca_thread_t id;

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

int wayca_thread_create(wayca_thread_t *wthread, pthread_attr_t *attr,
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

int wayca_thread_join(wayca_thread_t id, void **retval)
{
	struct wayca_thread *thread;
	int ret;

	if (!is_thread_id_valid(id))
		return -1; /* Invalid id */

	thread = id_to_wayca_thread(id);

	ret = pthread_join(thread->thread, retval);

	if (thread->group)
		wayca_thread_detach_group(id, thread->group->id);
	
	wayca_thread_update_load(thread, false);

	wayca_thread_free(thread);

	return ret;
}

int wayca_thread_group_set_attr(wayca_group_t group, wayca_group_attr_t *attr)
{
	struct wayca_group *wg_p;

	if (!is_group_id_valid(group))
		return -1;

	wg_p = id_to_wayca_group(group);
	wg_p->attribute = *attr;

	return wayca_group_rearrange_group(wg_p);
}

int wayca_thread_group_get_attr(wayca_group_t group, wayca_group_attr_t *attr)
{
	struct wayca_group *wg_p;

	if (!is_group_id_valid(group))
		return -1;

	wg_p = id_to_wayca_group(group);
	*attr = wg_p->attribute;

	return 0;
}

struct wayca_group *wayca_group_alloc()
{
	wayca_group_t id;

	pthread_mutex_lock(&wayca_groups_array_mutex);
	id = find_free_group_id_locked();
	if (id == -1)
		goto err;

	wayca_groups_array[id] = malloc(sizeof(struct wayca_group));
	if (!wayca_groups_array[id])
		goto err;

	pthread_mutex_unlock(&wayca_groups_array_mutex);

	memset(wayca_groups_array[id], 0, sizeof(struct wayca_group));
	wayca_groups_array[id]->id = id;

	return wayca_groups_array[id];
err:
	pthread_mutex_unlock(&wayca_groups_array_mutex);
	return NULL;
}

void wayca_group_free(struct wayca_group *group)
{
	pthread_mutex_lock(&wayca_groups_array_mutex);
	wayca_groups_array[group->id] = NULL;
	free(group);
	pthread_mutex_unlock(&wayca_groups_array_mutex);
}

int wayca_thread_group_create(wayca_group_t *group)
{
	struct wayca_group *wg_p;

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

int wayca_thread_group_destroy(wayca_group_t group)
{
	struct wayca_group *wg_p;

	if (!is_group_id_valid(group))
		return -1;

	wg_p = id_to_wayca_group(group);

	/* We still have threads in this group, stop destroy */
	if (wg_p->nr_threads)
		return -1;

	wayca_group_free(wg_p);

	return 0;
}

int wayca_thread_attach_group(wayca_thread_t wthread, wayca_group_t group)
{
	struct wayca_thread *wt_p;
	struct wayca_group *wg_p;

	if (!is_thread_id_valid(wthread) || !is_group_id_valid(group))
		return -1;

	wt_p = id_to_wayca_thread(wthread);
	wg_p = id_to_wayca_group(group);

	wayca_thread_update_load(wt_p, false);

	if (wayca_group_add_thread(wg_p, wt_p))
		return -1;

	return wayca_group_rearrange_thread(wg_p, wt_p);
}

int wayca_thread_detach_group(wayca_thread_t wthread, wayca_group_t group)
{
	struct wayca_thread *wt_p;
	struct wayca_group *wg_p;

	if (!is_thread_id_valid(wthread) || !is_group_id_valid(group))
		return -1;

	wt_p = id_to_wayca_thread(wthread);
	wg_p = id_to_wayca_group(group);

	return wayca_group_delete_thread(wg_p, wt_p);
}

int wayca_group_attach_group(wayca_group_t group, wayca_group_t father)
{
	struct wayca_group *wg_p, *father_p;

	if (!is_group_id_valid(group) || !is_group_id_valid(father))
		return -1;

	wg_p = id_to_wayca_group(group);
	father_p = id_to_wayca_group(father);

	/* If @group already has a father, should detach first before attach to a new one. */
	if (wg_p->father)
		return -1;

	return wayca_group_add_group(wg_p, father_p);
}

int wayca_group_detach_group(wayca_group_t group, wayca_group_t father)
{
	struct wayca_group *wg_p, *father_p;

	if (!is_group_id_valid(group) || !is_group_id_valid(father))
		return -1;

	wg_p = id_to_wayca_group(group);
	father_p = id_to_wayca_group(father);

	return wayca_group_delete_group(wg_p, father_p);
}

int wayca_thread_get_cpuset(wayca_thread_t wthread, cpu_set_t *cpuset)
{
	struct wayca_thread *wt_p;

	if (!is_thread_id_valid(wthread))
		return -1;

	wt_p = id_to_wayca_thread(wthread);

	CPU_ZERO(cpuset);
	CPU_OR(cpuset, cpuset, &wt_p->cur_set);

	return 0;
}

int wayca_group_get_cpuset(wayca_group_t group, cpu_set_t *cpuset)
{
	struct wayca_group *wg_p;

	if (!is_group_id_valid(group))
		return -1;

	wg_p = id_to_wayca_group(group);

	CPU_ZERO(cpuset);
	CPU_OR(cpuset, cpuset, &wg_p->total);

	return 0;
}

pthread_t wayca_thread_get_pthtread(wayca_thread_t wthread)
{
	struct wayca_thread *thread;

	if (!is_thread_id_valid(wthread))
		return -1; /* Invalid id */

	thread = id_to_wayca_thread(wthread);
	return thread->thread;
}

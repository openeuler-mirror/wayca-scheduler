#define _GNU_SOURCE
#include <sched.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/syscall.h>
#include <wayca-scheduler.h>

#define thread_sched_setaffinity(pid, size, cpuset) \
  syscall(__NR_sched_setaffinity, (pid_t)pid, (size_t)size, (void *)cpuset)
#define thread_sched_getaffinity(pid, size, cpuset) \
  syscall(__NR_sched_getaffinity, (pid_t)pid, (size_t)size, (void *)cpuset)

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
int process_bind_cpulist(pid_t pid, char *s)
{
	cpu_set_t mask;
	if (list_to_mask(s, &mask))
		return -1;
	return process_sched_setaffinity(pid, sizeof(mask), &mask);
}

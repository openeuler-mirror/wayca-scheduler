#ifndef LIB_COMMON_H_
#define LIB_COMMON_H_

#define _GNU_SOURCE
#include <sched.h>

static const char *wayca_scheduler_socket_path = "/etc/wayca-scheduler/wayca.socket";

/* default configurations */
#define MAX_MANAGED_MAPS	100
#define SOCKET_PATH		wayca_scheduler_socket_path

/* leverage the bitmap of cpu_set_t */
#define node_set_t cpu_set_t
#define NODE_ZERO CPU_ZERO
#define NODE_SET  CPU_SET
#define NODE_ISSET CPU_ISSET

#define task_set_t cpu_set_t
#define TASK_ZERO CPU_ZERO
#define TASK_SET  CPU_SET
#define TASK_ISSET CPU_ISSET

struct task_cpu_map
{
	task_set_t tasks;
	cpu_set_t cpus;
	node_set_t nodes;
	int cpu_util;
};

int list_to_mask(char *s, cpu_set_t *mask);
int to_task_cpu_map(char *cpu_list, struct task_cpu_map maps[]);
int process_bind_cpulist(pid_t pid, char *s);
int thread_bind_cpulist(pid_t pid, char *s);
int process_bind_cpumask(pid_t pid, cpu_set_t *cpumask, size_t maxCpus);
int thread_bind_cpumask(pid_t pid, cpu_set_t *cpumask, size_t maxCpus);

#endif

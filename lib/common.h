#ifndef LIB_COMMON_H_
#define LIB_COMMON_H_

#define _GNU_SOURCE
#include <stdbool.h>
#include <linux/limits.h>
#include <errno.h>
#include <sched.h>

extern char *wayca_scheduler_socket_path;

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

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define WAYCA_SC_PRIO_TOPO 101
#define WAYCA_SC_PRIO_THREAD 120
#define WAYCA_SC_PRIO_MANAGED_THREAD 110
#define WAYCA_SC_PRIO_LAST 65535

#define WAYCA_SC_PRIO(prio) \
	WAYCA_SC_PRIO_ ## prio

#define WAYCA_SC_INIT_PRIO(func, prio) \
static void __attribute__ ((constructor(WAYCA_SC_PRIO(prio)), used)) func(void)

#define WAYCA_SC_FINI_PRIO(func, prio) \
static void __attribute__ ((destructor(WAYCA_SC_PRIO(prio)), used)) func(void)

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

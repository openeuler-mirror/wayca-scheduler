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

#ifndef LIB_COMMON_H_
#define LIB_COMMON_H_

#define _GNU_SOURCE
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>

#include "wayca-scheduler.h"

extern char *wayca_scheduler_socket_path;

#ifdef WAYCA_SC_DEBUG
#include <assert.h>
#define WAYCA_SC_ASSERT(cond)	assert(cond)
#else
#define WAYCA_SC_ASSERT(cond)	do { } while (0)
#endif

/* default configurations */
#define MAX_MANAGED_MAPS	100
#define SOCKET_PATH		wayca_scheduler_socket_path

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

struct task_cpu_map {
	task_set_t tasks;
	cpu_set_t cpus;
	node_set_t nodes;
	int cpu_util;
};

int list_to_mask(char *s, size_t cpusetsize, cpu_set_t *mask);
int to_task_cpu_map(char *cpu_list, struct task_cpu_map maps[]);
int process_bind_cpulist(pid_t pid, char *s);
int thread_bind_cpulist(pid_t pid, char *s);
int process_bind_cpumask(pid_t pid, cpu_set_t *cpumask, size_t maxCpus);
int thread_bind_cpumask(pid_t pid, cpu_set_t *cpumask, size_t maxCpus);

int thread_bind_cpu(pid_t pid, int cpu);
int thread_bind_ccl(pid_t pid, int ccl);
int thread_bind_node(pid_t pid, int node);
int thread_bind_package(pid_t pid, int package);
int thread_unbind(pid_t pid);
int process_bind_cpu(pid_t pid, int cpu);
int process_bind_ccl(pid_t pid, int ccl);
int process_bind_node(pid_t pid, int node);
int process_bind_package(pid_t pid, int package);
int process_unbind(pid_t pid);

#endif

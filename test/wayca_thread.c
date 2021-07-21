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
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <syscall.h>
#include <unistd.h>

#include <wayca-scheduler.h>

#define TEST_THREADS	10
wayca_sc_thread_t threads[TEST_THREADS] = { 0 };
wayca_sc_group_t group = 0;
pid_t threads_pid[TEST_THREADS];
int system_cpu_nr = CPU_SETSIZE;

bool has_env = false;
wayca_sc_group_attr_t topo = 0;
wayca_sc_group_attr_t method = WT_GF_PERCPU;
wayca_sc_group_attr_t relation = 0;

void *thread_func(void *private)
{
	int index = (wayca_sc_thread_t *)private - threads;
	pid_t pid = syscall(SYS_gettid);

	printf("This is thread %d, pid is %d\n", index, pid);
	threads_pid[index] = pid;

	while (true)
		sleep(0.5);

	return NULL;
}

void show_thread_affinity(int created)
{
	cpu_set_t cpuset;

	for (int index = 0; index < created; index++) {
		pid_t pid = threads_pid[index];

		// sched_getaffinity(pid, sizeof(cpuset), &cpuset);
		wayca_sc_thread_get_cpuset(threads[index], sizeof(cpu_set_t),
					   &cpuset);

		printf("index %d pid %d: ", index, pid);
		for (int j = 0; j < (system_cpu_nr + __NCPUBITS - 1) / __NCPUBITS; j++)
			printf("%#016lx,", cpuset.__bits[j]);
		printf("\b \n");
	}
}

static wayca_sc_group_attr_t topo_attrs[] = {
	WT_GF_CPU,
	WT_GF_CCL,
	WT_GF_NUMA,
	WT_GF_PACKAGE,
	WT_GF_ALL,
};

void read_environ()
{
	char *s = secure_getenv("WAYCA_THREAD_TOPO_LEVEL");

	if (s) {
		has_env = true;
		topo = atoi(s);
	}
}

int main(int argc, char *argv[])
{
	int ret, created;
	wayca_sc_group_attr_t group_attr = 0;

	system_cpu_nr = wayca_sc_cpus_in_total();
	read_environ();

	ret = wayca_sc_group_create(&group);
	if (ret)
		return -1;

	for (created = 0; created < TEST_THREADS; created++) {
		ret = wayca_sc_thread_create(&threads[created], NULL, thread_func, &threads[created]);
		if (ret)
			break;
	}

	sleep(5);

	if (has_env) {
		group_attr = topo | method | relation;
		wayca_sc_group_set_attr(group, &group_attr);

		for (int index = 0; index < created; index++)
			wayca_sc_thread_attach_group(threads[index], group);

		show_thread_affinity(created);

		for (int index = 0; index < created; index++)
			wayca_sc_thread_detach_group(threads[index], group);
	}
	else
		for (int i = 0; i < sizeof(topo_attrs) / sizeof(wayca_sc_group_attr_t); i++)
		{
			topo = topo_attrs[i];
			method = WT_GF_PERCPU;
			relation = 0;
			// relation = WT_GF_COMPACT;

			printf("Topo: %lld Method: %lld Relation: %lld\n", topo, method, relation);
			group_attr = topo | method | relation;

			wayca_sc_group_set_attr(group, &group_attr);
			for (int index = 0; index < created; index++)
				wayca_sc_thread_attach_group(threads[index], group);

			show_thread_affinity(created);

			for (int index = 0; index < created; index++)
				wayca_sc_thread_detach_group(threads[index], group);
		}

	for (int i = 0; i < created; i++)
		wayca_sc_thread_join(threads[i], NULL);

	return 0;
}

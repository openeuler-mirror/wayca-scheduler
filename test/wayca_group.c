#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <stdbool.h>
#include <syscall.h>
#include <unistd.h>

#include <wayca-scheduler.h>

int group_num = 11;
int group_elem_num = 11;

wayca_group_t all, *perCcl;
wayca_thread_t **threads;
pid_t **threads_pid;

int system_cpu_nr;

struct wayca_thread_info {
	wayca_thread_t wthread;
	wayca_group_t wgroup;
};

struct wayca_thread_info **global_info;

bool quit = false;

void show_thread_affinity()
{
	cpu_set_t cpuset;

	for (int i = 0; i < group_num; i++)
		for (int j = 0; j < group_elem_num; j++) {
			pid_t pid = threads_pid[i][j];

			// sched_getaffinity(pid, sizeof(cpuset), &cpuset);
			wayca_thread_get_cpuset(threads[i][j], &cpuset);

			printf("group %d thread %d pid %d: ", i, j, pid);
			for (int b = 0; b < (system_cpu_nr + __NCPUBITS - 1) / __NCPUBITS; b++)
				printf("0x%016lx,", cpuset.__bits[b]);
			printf("\b \n");
	}
}

void *thread_func(void *private)
{
	struct wayca_thread_info *info = private;
	pid_t pid = syscall(SYS_gettid);

	threads_pid[info->wgroup][info->wthread] = pid;

	printf("group %lld thread %lld pid %d\n", info->wgroup, info->wthread, pid);

	while (!quit)
		sleep(0.5);

	return NULL;
}

int main()
{
	wayca_group_attr_t group_attr;
	int i, j;
	char *p;

	p = getenv("WAYCA_TEST_GROUPS");
	if (p)
		group_num = atoi(p);
	p = getenv("WAYCA_TEST_GROUP_ELEMS");
	if (p)
		group_elem_num = atoi(p);

	perCcl = malloc(group_num * sizeof(wayca_group_t));
	threads = malloc(group_num * sizeof(wayca_thread_t *));
	threads_pid = malloc(group_num * sizeof(pid_t *));
	global_info = malloc(group_num * sizeof(struct wayca_thread_info *));

	for (i = 0; i < group_num; i++) {
		threads[i] = malloc(group_elem_num * sizeof(wayca_thread_t));
		threads_pid[i] = malloc(group_elem_num * sizeof(pid_t));
		global_info[i] = malloc(group_elem_num * sizeof(struct wayca_thread_info));
	}


	system_cpu_nr = cores_in_total();

	group_attr = WT_GF_NUMA | WT_GF_FIXED;

	wayca_thread_group_create(&all);
	wayca_thread_group_set_attr(all, &group_attr);

	group_attr = WT_GF_CCL | WT_GF_FIXED;

	for (i = 0; i < group_num; i++) {
		wayca_thread_group_create(&perCcl[i]);
		wayca_thread_group_set_attr(perCcl[i], &group_attr);

		for (j = 0; j < group_elem_num; j++) {
			global_info[i][j] = (struct wayca_thread_info) {
				.wgroup = i,
				.wthread = j,
			};
			wayca_thread_create(&threads[i][j], NULL, thread_func, &global_info[i][j]);

			sleep(0.5);

			wayca_thread_attach_group(threads[i][j], perCcl[i]);
		}

		wayca_group_attach_group(perCcl[i], all);
	}

	sleep(5);
	show_thread_affinity();

	quit = true;

	for (i = 0; i < group_num; i++) {
		for (j = 0; j < group_elem_num; j++) {
			wayca_thread_detach_group(threads[i][j], perCcl[i]);
			wayca_thread_join(threads[i][j], NULL);
		}

		wayca_group_detach_group(perCcl[i], all);
		wayca_thread_group_destroy(perCcl[i]);
	}

	for (i = 0; i < group_num; i++) {
		free(threads[i]);
		free(threads_pid[i]);
		free(global_info[i]);
	}

	free(threads);
	free(threads_pid);
	free(global_info);

	return 0;
}

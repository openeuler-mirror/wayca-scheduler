#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <stdbool.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include <wayca-scheduler.h>

int group_num = 11;
int group_elem_num = 11;

wayca_sc_group_t all, *perCcl;
wayca_sc_group_attr_t all_attr, perCcl_attr;
wayca_thread_t **threads;
pid_t **threads_pid;

int system_cpu_nr;

struct wayca_thread_info {
	wayca_thread_t wthread;
	wayca_sc_group_t wgroup;
};

struct wayca_thread_info **global_info;

bool quit = false;

void show_thread_affinity()
{
	cpu_set_t cpuset;

	for (int i = 0; i < group_num; i++) {
		for (int j = 0; j < group_elem_num; j++) {
			pid_t pid = threads_pid[i][j];

			// sched_getaffinity(pid, sizeof(cpuset), &cpuset);
			wayca_thread_get_cpuset(threads[i][j], &cpuset);

			printf("group %d thread %d pid %d:\t", i, j, pid);
			for (int b = 0; b < (system_cpu_nr + __NCPUBITS - 1) / __NCPUBITS; b++)
				printf("0x%08lx,0x%08lx,", cpuset.__bits[b] & 0xffffffff, cpuset.__bits[b] >> 32);
			printf("\b \n");
		}
		printf("\n");
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

const char *topo_level[] = {
	"CPU", "CCL", "NUMA", "PACKAGE",
};

void readEnv(void)
{
	char *p;

	p = getenv("WAYCA_TEST_GROUPS");
	if (p)
		group_num = atoi(p);
	p = getenv("WAYCA_TEST_GROUP_ELEMS");
	if (p)
		group_elem_num = atoi(p);

	p = getenv("WAYCA_TEST_GROUP_TOPO_LEVEL");
	if (!p)
		all_attr |= WT_GF_NUMA;
	else if (!strcmp(p, topo_level[0]))
		all_attr |= WT_GF_CPU;
	else if (!strcmp(p, topo_level[1]))
		all_attr |= WT_GF_CCL;
	else if (!strcmp(p, topo_level[2]))
		all_attr |= WT_GF_NUMA;
	else if (!strcmp(p, topo_level[3]))
		all_attr |= WT_GF_PACKAGE;
	else
		all_attr |= WT_GF_NUMA;

	p = getenv("WAYCA_TEST_THREAD_TOPO_LEVEL");
	if (!p)
		perCcl_attr |= WT_GF_CCL;
	else if (!strcmp(p, topo_level[0]))
		perCcl_attr |= WT_GF_CPU;
	else if (!strcmp(p, topo_level[1]))
		perCcl_attr |= WT_GF_CCL;
	else if (!strcmp(p, topo_level[2]))
		perCcl_attr |= WT_GF_NUMA;
	else if (!strcmp(p, topo_level[3]))
		perCcl_attr |= WT_GF_PACKAGE;
	else
		perCcl_attr |= WT_GF_CCL;
	
	p = getenv("WAYCA_TEST_THREAD_BIND_PERCPU");
	if (p)
		perCcl_attr |= WT_GF_PERCPU;

	p = getenv("WAYCA_TEST_THREAD_COMPACT");
	if (p)
		perCcl_attr |= WT_GF_COMPACT;
}

int main()
{
	int i, j, group_created, group_elem_created, ret = 0;
	wayca_sc_group_attr_t group_attr;

	readEnv();

	perCcl = malloc(group_num * sizeof(wayca_sc_group_t));
	threads = malloc(group_num * sizeof(wayca_thread_t *));
	threads_pid = malloc(group_num * sizeof(pid_t *));
	global_info = malloc(group_num * sizeof(struct wayca_thread_info *));

	if (!perCcl || !threads || !threads_pid || !global_info)
		return -ENOMEM;

	for (i = 0; i < group_num; i++) {
		threads[i] = malloc(group_elem_num * sizeof(wayca_thread_t));
		threads_pid[i] = malloc(group_elem_num * sizeof(pid_t));
		global_info[i] = malloc(group_elem_num * sizeof(struct wayca_thread_info));

		if (!threads[i] || !threads_pid[i] || !global_info[i]) {
			ret = -ENOMEM;
			group_num = i;
			goto err_wayca_info;
		}
	}

	system_cpu_nr = cores_in_total();

	group_attr = all_attr;

	ret = wayca_sc_group_create(&all);
	if (ret)
		goto err_wayca_info;

	ret = wayca_sc_group_set_attr(all, &group_attr);
	if (ret)
		goto err_wayca_info;

	group_attr = perCcl_attr;

	for (group_created = 0; group_created < group_num; group_created++) {
		wayca_sc_group_create(&perCcl[group_created]);
		wayca_sc_group_set_attr(perCcl[group_created], &group_attr);

		for (group_elem_created = 0; group_elem_created < group_elem_num; group_elem_created++)
		{
			global_info[group_created][group_elem_created] = (struct wayca_thread_info) {
				.wgroup = group_created,
				.wthread = group_elem_created,
			};
			ret = wayca_thread_create(&threads[group_created][group_elem_created],
						  NULL, thread_func,
						  &global_info[group_created][group_elem_created]);
			if (ret)
				goto err_wayca_threads;

			ret = wayca_thread_attach_group(threads[group_created][group_elem_created],
							perCcl[group_created]);
			if (ret)
				goto err_wayca_threads;
		}

		ret = wayca_sc_group_attach_group(perCcl[group_created], all);
		if (ret)
			goto err_wayca_threads;
	}

	sleep(5);
	show_thread_affinity();

	quit = true;

err_wayca_threads:
	for (i = 0; i < group_created; i++) {
		for (j = 0; j < group_elem_created; j++) {
			wayca_thread_detach_group(threads[i][j], perCcl[i]);
			wayca_thread_join(threads[i][j], NULL);
		}

		wayca_sc_group_detach_group(perCcl[i], all);
		wayca_sc_group_destroy(perCcl[i]);
	}

err_wayca_info:
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

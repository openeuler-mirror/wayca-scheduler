#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <wayca-scheduler.h>

#define TEST_THREADS	10
#define TEST_GROUPS	10
wayca_group_t father;
wayca_group_t groups[TEST_GROUPS] = { 0 };
wayca_thread_t threads[TEST_GROUPS][TEST_THREADS];
int system_cpu_nr = CPU_SETSIZE;

void *thread_func(void *private)
{
	while (true)
		sleep(0.5);

	return NULL;
}

void show_group_affinity(wayca_group_t group)
{
	cpu_set_t cpuset;

	wayca_group_get_cpuset(group, &cpuset);

	printf("group %d: ", group);
	for (int j = 0; j < (system_cpu_nr + __NCPUBITS - 1) / __NCPUBITS; j++)
		printf("0x%016llx,", cpuset.__bits[j]);
	printf("\b \n");
}

int main(int argc, char *argv[])
{
	int ret, created;
	wayca_group_attr_t group_attr = 0;

	system_cpu_nr = cores_in_total();

	ret = wayca_thread_group_create(&father);
	if (ret)
		return -1;
	group_attr = WT_GF_CCL | WT_GF_FIXED;
	wayca_thread_group_set_attr(father, &group_attr);

	show_group_affinity(father);

	group_attr = WT_GF_CPU | WT_GF_COMPACT | WT_GF_FIXED;

	for (created = 0; created < TEST_GROUPS; created++) {
		ret = wayca_thread_group_create(&groups[created]);
		if (ret < 0)
			break;
		wayca_thread_group_set_attr(groups[created], &group_attr);

		for (int t = 0; t < TEST_THREADS; t++) {
			wayca_thread_create(&threads[created][t], NULL, thread_func, &threads[created][t]);
			wayca_thread_attach_group(threads[created][t], groups[created]);
		}

		wayca_group_attach_group(groups[created], father);

		show_group_affinity(groups[created]);
	}

	for (int i = 0; i < created; i--)
		for (int t = 0; t < TEST_THREADS; t++)
			wayca_thread_join(threads[i][t], NULL);

	return 0;
}

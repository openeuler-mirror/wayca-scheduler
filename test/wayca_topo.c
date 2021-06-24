/* wayca_topo.c
 * Author: Guodong Xu <guodong.xu@linaro.org>
 * License: GPLv2
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include "wayca-scheduler.h"

#define TEST_INVALID_ID -1
static void test_entity_number()
{
	printf("cpus_in_ccl: %d\n", wayca_sc_cpus_in_ccl());
	printf("cpus_in_node: %d\n", wayca_sc_cpus_in_node());
	printf("cpus_in_package: %d\n", wayca_sc_cpus_in_package());
	printf("cpus_in_total: %d\n", wayca_sc_cpus_in_total());
	printf("ccls_in_node: %d\n", wayca_sc_ccls_in_node());
	printf("ccls_in_package: %d\n", wayca_sc_ccls_in_package());
	printf("ccls_in_total: %d\n", wayca_sc_ccls_in_total());
	printf("nodes_in_package: %d\n", wayca_sc_nodes_in_package());
	printf("nodes_in_total: %d\n", wayca_sc_nodes_in_total());
	printf("packages_in_total: %d\n", wayca_sc_packages_in_total());
}

static void test_get_entity_id()
{
	int ret;
	/* abornormal case */
	ret = wayca_sc_get_core_id(TEST_INVALID_ID);
	assert(ret < 0);
	ret = wayca_sc_get_ccl_id(TEST_INVALID_ID);
	assert(ret < 0);
	ret = wayca_sc_get_node_id(TEST_INVALID_ID);
	assert(ret < 0);
	ret = wayca_sc_get_package_id(TEST_INVALID_ID);
	assert(ret < 0);

	/* normal case*/
	ret = wayca_sc_get_package_id(0);
	assert(ret >= 0);
	ret = wayca_sc_get_node_id(0);
	assert(ret >= 0);
	ret = wayca_sc_get_ccl_id(0);
	assert(ret == -EINVAL || ret >= 0);
	ret = wayca_sc_get_core_id(0);
	assert(ret >= 0);
}

static void print_cpumask(const char *topo, size_t setsize, cpu_set_t *mask)
{

	printf("%s cpucount:", topo);
	printf("%d\n", CPU_COUNT_S(setsize, mask));
}

static void test_get_cpu_list()
{
	int n_cpus = wayca_sc_cpus_in_total();
	cpu_set_t *cpu_set;
	size_t setsize;
	int ret;

	setsize = CPU_ALLOC_SIZE(n_cpus);
	cpu_set = CPU_ALLOC(n_cpus);

	ret = wayca_sc_ccl_cpu_mask(0, 0, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_node_cpu_mask(0, 0, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_package_cpu_mask(0, 0, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_total_cpu_mask(0, cpu_set);
	assert(ret < 0);

	ret = wayca_sc_ccl_cpu_mask(TEST_INVALID_ID, setsize, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_node_cpu_mask(TEST_INVALID_ID, setsize, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_package_cpu_mask(TEST_INVALID_ID, setsize, cpu_set);
	assert(ret < 0);

	ret = wayca_sc_ccl_cpu_mask(0, setsize, cpu_set);
	assert(ret == 0 || ret == -EINVAL);
	if (ret == 0)
		print_cpumask("cluster 0",setsize, cpu_set);
	ret = wayca_sc_node_cpu_mask(0, setsize, cpu_set);
	assert(ret == 0);
	print_cpumask("node 0", setsize, cpu_set);
	ret = wayca_sc_package_cpu_mask(0, setsize, cpu_set);
	assert(ret == 0);
	print_cpumask("package 0", setsize, cpu_set);
	ret = wayca_sc_total_cpu_mask(setsize, cpu_set);
	assert(ret == 0);
	print_cpumask("total", setsize, cpu_set);
}

static void test_get_io_info()
{
	unsigned long int size;
	int ret;

	ret = wayca_sc_get_node_mem_size(TEST_INVALID_ID, &size);
	assert(ret < 0);

	ret = wayca_sc_get_node_mem_size(0, &size);
	assert(ret >= 0);
}

int main()
{
	wayca_sc_topo_print();

	test_entity_number();
	test_get_entity_id();
	test_get_cpu_list();
	test_get_io_info();

	return 0;
}

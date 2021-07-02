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

/* wayca_topo.c
 * Author: Guodong Xu <guodong.xu@linaro.org>
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
	int ret;

	ret = wayca_sc_cpus_in_ccl();
	assert(ret > 0 || ret == -ENODATA);
	if (ret != -ENODATA)
		printf("cpus_in_ccl: %d\n", ret);
	ret = wayca_sc_cpus_in_node();
	assert(ret > 0);
	printf("cpus_in_node: %d\n", ret);
	ret = wayca_sc_cpus_in_package();
	assert(ret > 0);
	printf("cpus_in_package: %d\n", ret);
	ret = wayca_sc_cpus_in_total();
	assert(ret > 0);
	printf("cpus_in_total: %d\n", ret);

	ret = wayca_sc_ccls_in_node();
	assert(ret > 0 || ret == -ENODATA);
	if (ret != -ENODATA)
		printf("ccls_in_node: %d\n", ret);
	ret = wayca_sc_ccls_in_package();
	assert(ret > 0 || ret == -ENODATA);
	if (ret != -ENODATA)
		printf("ccls_in_package: %d\n", ret);
	ret = wayca_sc_ccls_in_total();
	assert(ret > 0 || ret == -ENODATA);
	if (ret != -ENODATA)
		printf("ccls_in_total: %d\n", ret);

	ret = wayca_sc_nodes_in_package();
	assert(ret > 0);
	printf("nodes_in_package: %d\n", ret);
	ret = wayca_sc_nodes_in_total();
	assert(ret > 0);
	printf("nodes_in_total: %d\n", ret);

	ret = wayca_sc_packages_in_total();
	assert(ret > 0);
	printf("packages_in_total: %d\n", ret);
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
	printf("package logic id of cpu 0: %d\n", ret);
	ret = wayca_sc_get_node_id(0);
	assert(ret >= 0);
	printf("numa node logic id of cpu 0: %d\n", ret);
	ret = wayca_sc_get_ccl_id(0);
	assert(ret == -EINVAL || ret >= 0);
	if (ret >= 0)
		printf("cluster logic id of cpu 0: %d\n", ret);
	ret = wayca_sc_get_core_id(0);
	assert(ret >= 0);
	printf("core logic id of cpu 0: %d\n", ret);
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

static void test_get_cache_info()
{
	int ret;

	ret = wayca_sc_get_l1d_size(TEST_INVALID_ID);
	assert(ret < 0);
	ret = wayca_sc_get_l1i_size(TEST_INVALID_ID);
	assert(ret < 0);
	ret = wayca_sc_get_l2_size(TEST_INVALID_ID);
	assert(ret < 0);
	ret = wayca_sc_get_l3_size(TEST_INVALID_ID);
	assert(ret < 0);

	ret = wayca_sc_get_l1d_size(0);
	assert(ret > 0);
	printf("core 0 L1 data cache: %dKB\n", ret);
	ret = wayca_sc_get_l1i_size(0);
	assert(ret > 0);
	printf("core 0 L1 instruction cache: %dKB\n", ret);
	ret = wayca_sc_get_l2_size(0);
	assert(ret > 0);
	printf("core 0 L2 cache: %dKB\n", ret);
	ret = wayca_sc_get_l3_size(0);
	assert(ret > 0);
	printf("core 0 L3 cache: %dKB\n", ret);
}

int main()
{
	wayca_sc_topo_print();

	test_entity_number();
	test_get_entity_id();
	test_get_cpu_list();
	test_get_cache_info();
	test_get_io_info();

	return 0;
}

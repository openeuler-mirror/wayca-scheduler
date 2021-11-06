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
	ret = wayca_sc_cpus_in_core();
	assert(ret > 0);
	printf("cpus_in_core: %d\n", ret);
	ret = wayca_sc_cpus_in_node();
	assert(ret > 0);
	printf("cpus_in_node: %d\n", ret);
	ret = wayca_sc_cpus_in_package();
	assert(ret > 0);
	printf("cpus_in_package: %d\n", ret);
	ret = wayca_sc_cpus_in_total();
	assert(ret > 0);
	printf("cpus_in_total: %d\n", ret);

	ret = wayca_sc_cores_in_ccl();
	assert(ret > 0 || ret == -ENODATA);
	if (ret != -ENODATA)
		printf("cores_in_ccl: %d\n", ret);
	ret = wayca_sc_cores_in_node();
	assert(ret > 0);
	printf("cores_in_node: %d\n", ret);
	ret = wayca_sc_cores_in_package();
	assert(ret > 0);
	printf("cores_in_package: %d\n", ret);
	ret = wayca_sc_cores_in_total();
	assert(ret > 0);
	printf("cores_in_total: %d\n", ret);

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

static void print_nodemask(const char *topo, size_t setsize, cpu_set_t *mask)
{

	printf("%s nodecount:", topo);
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

	ret = wayca_sc_core_cpu_mask(0, 0, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_ccl_cpu_mask(0, 0, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_node_cpu_mask(0, 0, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_package_cpu_mask(0, 0, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_total_cpu_mask(0, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_package_node_mask(0, 0, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_total_node_mask(0, cpu_set);
	assert(ret < 0);

	ret = wayca_sc_core_cpu_mask(TEST_INVALID_ID, setsize, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_ccl_cpu_mask(TEST_INVALID_ID, setsize, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_node_cpu_mask(TEST_INVALID_ID, setsize, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_package_cpu_mask(TEST_INVALID_ID, setsize, cpu_set);
	assert(ret < 0);
	ret = wayca_sc_package_node_mask(TEST_INVALID_ID, setsize, cpu_set);
	assert(ret < 0);

	ret = wayca_sc_ccl_cpu_mask(0, setsize, cpu_set);
	assert(ret == 0 || ret == -EINVAL);
	if (ret == 0)
		print_cpumask("cluster 0",setsize, cpu_set);
	ret = wayca_sc_core_cpu_mask(0, setsize, cpu_set);
	assert(ret == 0);
	print_cpumask("cluster 0", setsize, cpu_set);
	ret = wayca_sc_node_cpu_mask(0, setsize, cpu_set);
	assert(ret == 0);
	print_cpumask("node 0", setsize, cpu_set);
	ret = wayca_sc_package_cpu_mask(0, setsize, cpu_set);
	assert(ret == 0);
	print_cpumask("package 0", setsize, cpu_set);
	ret = wayca_sc_total_cpu_mask(setsize, cpu_set);
	assert(ret == 0);
	print_cpumask("total", setsize, cpu_set);

	ret = wayca_sc_package_node_mask(0, setsize, cpu_set);
	assert(ret == 0);
	print_nodemask("package 0", setsize, cpu_set);
	ret = wayca_sc_total_node_mask(setsize, cpu_set);
	assert(ret == 0);
	print_nodemask("total", setsize, cpu_set);
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

static void test_get_device_info(void)
{
	struct wayca_sc_device_info dev_info = {0};
	const char **dev_name;
	size_t num;
	int ret;

	/* normal case */
	ret = wayca_sc_get_device_list(0, &num, NULL);
	assert(ret == 0);
	ret = wayca_sc_get_device_list(-1, &num, NULL);
	assert(ret == 0);
	dev_name = (const char **)calloc(num, sizeof(char *));
	assert(dev_name != NULL);
	ret = wayca_sc_get_device_list(-1, &num, dev_name);
	assert(ret == 0);

	/* abnormal case */
	ret = wayca_sc_get_device_list(0, NULL, dev_name);
	assert(ret < 0);
	ret = wayca_sc_get_device_list(0, NULL, NULL);
	assert(ret < 0);

	/* normal case */
	ret = wayca_sc_get_device_info(dev_name[0], &dev_info);
	assert(ret == 0);

	/* abnormal case */
	ret = wayca_sc_get_device_info(NULL, &dev_info);
	assert(ret < 0);
	ret = wayca_sc_get_device_info(dev_name[0], NULL);
	assert(ret < 0);
	free(dev_name);
	printf("get device info successful.\n");
}

static void test_get_irq_info(void)
{
#define TEST_INVALID_IRQ 100000
	struct wayca_sc_irq_info irq_info = {0};
	uint32_t *irq;
	size_t num;
	int ret;

	/* normal case */
	ret = wayca_sc_get_irq_list(&num, NULL);
	assert(ret == 0);
	irq = (uint32_t *)calloc(num, sizeof(uint32_t));
	assert(irq != NULL);
	ret = wayca_sc_get_irq_list(&num, irq);
	assert(ret == 0);

	/* abnormal case */
	ret = wayca_sc_get_irq_list(NULL, NULL);
	assert(ret < 0);
	ret = wayca_sc_get_irq_list(NULL, irq);
	assert(ret < 0);

	/* normal case */
	ret = wayca_sc_get_irq_info(irq[0], &irq_info);
	assert(ret == 0);

	/* abnormal case */
	ret = wayca_sc_get_irq_info(irq[0], NULL);
	assert(ret < 0);
	ret = wayca_sc_get_irq_info(TEST_INVALID_IRQ, &irq_info);
	assert(ret < 0);

	free(irq);
	printf("get IRQ info successful.\n");
}

int main()
{
	wayca_sc_topo_print();

	test_entity_number();
	test_get_entity_id();
	test_get_cpu_list();
	test_get_cache_info();
	test_get_io_info();
	test_get_device_info();
	test_get_irq_info();

	return 0;
}

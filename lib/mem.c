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
#include <limits.h>
#include <linux/mempolicy.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <wayca-scheduler.h>

#include "common.h"

static inline long set_mempolicy(int mode, const unsigned long *nodemask,
				 unsigned long maxnode)
{
	int ret;

	ret = syscall(__NR_set_mempolicy, mode, nodemask, maxnode);
	return  ret < 0 ? -errno : ret;
}

static inline long get_mempolicy(int *mode, unsigned long *nodemask,
				 unsigned long maxnode, void *addr,
				 unsigned long flags)
{
	int ret;

	ret = syscall(__NR_get_mempolicy, mode, nodemask,
		      maxnode, addr, flags);
	return ret < 0 ? -errno : ret;
}

static inline long migrate_pages(int pid, unsigned long maxnode,
				 const unsigned long *frommask,
				 const unsigned long *tomask)
{
	int ret;

	ret = syscall(__NR_migrate_pages, pid, maxnode, frommask, tomask);
	return ret < 0 ? -errno : ret;
}

static inline void set_node_mask(int node, node_set_t * mask)
{
	NODE_ZERO(mask);
	NODE_SET(node, mask);
}

int wayca_sc_mem_interleave_in_package(int package)
{
	node_set_t mask;
	int ret;

	ret = wayca_sc_package_node_mask(package, sizeof(node_set_t),
					 &mask);
	if (ret < 0)
		return ret;

	return set_mempolicy(MPOL_INTERLEAVE, (unsigned long *)&mask,
			     wayca_sc_nodes_in_total() + 1);
}

int wayca_sc_mem_interleave_in_all(void)
{
	node_set_t mask;
	int ret;

	ret = wayca_sc_total_node_mask(sizeof(node_set_t), &mask);
	if (ret < 0)
		return ret;

	return set_mempolicy(MPOL_INTERLEAVE, (unsigned long *)&mask,
			     wayca_sc_nodes_in_total() + 1);
}

int wayca_sc_mem_bind_node(int node)
{
	node_set_t mask;

	if (node < 0 || node >= wayca_sc_nodes_in_total())
		return -EINVAL;

	set_node_mask(node, &mask);
	return set_mempolicy(MPOL_BIND, (unsigned long *)&mask, wayca_sc_nodes_in_total() + 1);
}

int wayca_sc_mem_bind_package(int package)
{
	node_set_t mask;
	int ret;

	ret = wayca_sc_package_node_mask(package, sizeof(node_set_t),
					 &mask);
	if (ret < 0)
		return ret;

	return set_mempolicy(MPOL_BIND, (unsigned long *)&mask,
			     wayca_sc_nodes_in_total() + 1);
}

int wayca_sc_mem_unbind(void)
{
	return set_mempolicy(MPOL_DEFAULT, NULL, wayca_sc_nodes_in_total());
}

int wayca_sc_get_mem_bind_nodes(size_t maxnode, node_set_t *mask)
{
	int mode, ret;

	ret = get_mempolicy(&mode, (unsigned long *)mask, maxnode, NULL, 0);

	if (ret < 0)
		return ret;

	if (mode & (MPOL_BIND | MPOL_INTERLEAVE))
		return 0;

	return -ENODATA;
}

/*
 * On success it returns the number of pages that could not be moved (i.e., a return of
 * zero means that all pages were successfully moved). On error, it returns -1, and sets
 * errno to indicate the error.
 */
long wayca_sc_mem_migrate_to_node(pid_t pid, int node)
{
	node_set_t all_mask, node_mask;
	int ret;

	ret = wayca_sc_total_node_mask(sizeof(node_set_t), &all_mask);
	if (ret < 0)
		return ret;

	set_node_mask(node, &node_mask);
	return migrate_pages(pid, wayca_sc_nodes_in_total() + 1,
			     (unsigned long *)&all_mask,
			     (unsigned long *)&node_mask);
}

long wayca_sc_mem_migrate_to_package(pid_t pid, int package)
{
	node_set_t all_mask, pack_mask;
	int ret;

	ret = wayca_sc_total_node_mask(sizeof(node_set_t), &all_mask);
	if (ret < 0)
		return ret;

	ret = wayca_sc_package_node_mask(package, sizeof(node_set_t),
					 &pack_mask);
	if (ret < 0)
		return ret;

	return migrate_pages(pid, wayca_sc_nodes_in_total() + 1,
			     (unsigned long *)&all_mask,
			     (unsigned long *)&pack_mask);
}

#define _GNU_SOURCE
#include <sched.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <wayca-scheduler.h>

#include "common.h"

#define set_mempolicy(mode, nodemask, maxnode) \
	syscall(__NR_set_mempolicy, (int)mode, (unsigned long *)nodemask, (unsigned long)maxnode)

#define migrate_pages(pid, maxnode, frommask, tomask) \
	syscall(__NR_migrate_pages, (int)pid, (unsigned long)maxnode, \
		(const unsigned long *)frommask, (const unsigned long *)tomask);

enum {
	MPOL_DEFAULT,
	MPOL_PREFERRED,
	MPOL_BIND,
	MPOL_INTERLEAVE,
	MPOL_LOCAL,
	MPOL_MAX,
};

static inline void set_node_mask(int node, node_set_t * mask)
{
	NODE_ZERO(mask);
	NODE_SET(node, mask);
}

static inline void set_package_mask(int node, node_set_t * mask)
{
	int nr_in_pack = nodes_in_package();
	node = node / nr_in_pack * nr_in_pack;
	int i;

	NODE_ZERO(mask);
	for (i = 0; i < nr_in_pack; i++)
		NODE_SET(node + i, mask);
}

static inline void set_all_mask(node_set_t * mask)
{
	int i;

	NODE_ZERO(mask);
	for (i = 0; i < nodes_in_total(); i++)
		NODE_SET(i, mask);
}

int mem_interleave_in_package(int node)
{
	node_set_t mask;
	set_package_mask(node, &mask);
	return set_mempolicy(MPOL_INTERLEAVE, (unsigned long *)&mask,
			     nodes_in_total());
}

int mem_interleave_in_all(void)
{
	node_set_t mask;
	set_all_mask(&mask);
	return set_mempolicy(MPOL_INTERLEAVE, (unsigned long *)&mask,
			     nodes_in_total());
}

int mem_bind_node(int node)
{
	node_set_t mask;
	set_node_mask(node, &mask);
	return set_mempolicy(MPOL_BIND, (unsigned long *)&mask, node + 1);
}

int mem_bind_package(int node)
{
	node_set_t mask;
	set_package_mask(node, &mask);
	return set_mempolicy(MPOL_BIND, (unsigned long *)&mask,
			     nodes_in_total());
}

int mem_unbind(void)
{
	return set_mempolicy(MPOL_DEFAULT, NULL, nodes_in_total());
}

/*
 * On success it returns the number of pages that could not be moved (i.e., a return of
 * zero means that all pages were successfully moved). On error, it returns -1, and sets
 * errno to indicate the error.
 */
long mem_migrate_to_node(pid_t pid, int node)
{
	node_set_t all_mask, node_mask;
	set_all_mask(&all_mask);
	set_node_mask(node, &node_mask);
	return migrate_pages(pid, nodes_in_total(), &all_mask, &node_mask);
}

long mem_migrate_to_package(pid_t pid, int node)
{
	node_set_t all_mask, pack_mask;
	set_all_mask(&all_mask);
	set_package_mask(node, &pack_mask);
	return migrate_pages(pid, nodes_in_total(), &all_mask, &pack_mask);
}

#ifndef DEPLOY_COMMON_H
#define DEPLOY_COMMON_H

#include "lib/common.h"

#define MAX_IRQS_BIND 20

/*
 * memory bandwidth requirement of the application
 * LOW: we don't care about bandwidth, for this, we can organize process in one CCL
 * DIE: we are going to split threads to the NUMA node for low memory bandwidth
 * PACKAGE: we are going to use memory controllers in multiple NUMA nodes in one package
 * ALL: we are going to all memory controller
 */
enum MEMBAND {LOW, DIE, PACKAGE, ALL};

/*
 * CPU binding modes
 * AUTO: let wayca-deployd bind the whole process to CCL, NODE, PACKAGE based on IO node
 * COARSE: wayca-deployer won't differentiate each thread in one process
 * FINE: for threads created by wayca_managed_thread APIs, wayca-deployer can do fine-
 * grained binding for each thread or threadpool
 */
enum CPUBIND {AUTO, COARSE, FINE};

struct program {
	pid_t pid;
	char exec[PATH_MAX];
	char cpu_list[PATH_MAX];
	int irq_bind[MAX_IRQS_BIND][2];
	int cpu_util;
	int io_node;
	enum CPUBIND task_bind_mode;
	enum MEMBAND mem_band;
};

static inline int cfg_strtostr(char *buf, char *str)
{
	char *p;


	p = strchr(buf, '\n');
	*p = 0;

	p = strchr(buf, '=');
	if (!p)
		return -1;

	snprintf(str, PATH_MAX, "%s", p + 1);

	return 0;
}

static inline bool str_start_with(const char *str, const char *start)
{
	return !strncmp(str, start, strlen(start));
}
#endif

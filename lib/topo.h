/* topo.h - By design, this header file is supposed to be used only by topo.c.
 *  For anything which needs to be exported to external users, please move it
 *  to include/wayca-scheduler.h
 *
 * Author: Guodong Xu <guodong.xu@linaro.org>
 * License: GPLv2
 */

#ifndef _TOPO_H
#define _TOPO_H 	1

#include <sched.h>

#define NODE_FNAME 	"/sys/devices/system/node"
#define CPU_FNAME 	"/sys/devices/system/cpu"	/* no ending '/' in the filename */
#define DEFAULT_KERNEL_MAX 	(2048)
#define PATH_LEN_MAX		(4096)		/* maximum length of file pathname */ 

#define PRINT_DBG	printf
#define PRINT_ERROR	printf

struct wayca_cpu {
	int core_id;
	struct wayca_cluster	*p_cluster;	/* in which cluster */
	struct wayca_node	*p_numa_node;	/* in which Numa node */
	struct wayca_package	*p_package;	/* in which Package */
	cpu_set_t *core_cpus_map;		/* SMT - simultaneous multi-threading siblings; CPUs within the same core
						 *   (deprecated name: "thread_siblings_list"
						 */
};

struct wayca_cluster {
	int cluster_id;
	size_t n_cpus;		/* number of CPUs in this cluster */
	cpu_set_t *cpu_map;	/* mask of contained CPUs */
};

struct wayca_node {
	int node_idx;			/* index of node */
	size_t n_cpus;			/* number of CPUs in this numa node */
	cpu_set_t *cpu_map;		/* mask of contained CPUs */
	cpu_set_t *cluster_map;		/* mask of contained clusters */

	int *distance;			/* array of distance */
	struct wayca_meminfo	*p_meminfo;	/* memory information of this node */
};

struct wayca_meminfo {
	unsigned long total_avail_kB;	/* total available memory in kiloBytes */
};

struct wayca_package {
	int physical_package_id;
	size_t n_cpus;		/* number of CPUs in this cluster */
	cpu_set_t *cpu_map;		/* mask of contained CPUs */
	cpu_set_t *numa_map;		/* mask of contained numa nodes */
};

struct wayca_topo {
	int kernel_max_cpus;			/* maximum number of CPUs kernel can support */
	size_t setsize;				/* setsize for use in CPU_SET macros */

	size_t n_cpus;				/* total number of CPUs */
	cpu_set_t *cpu_map;
	struct wayca_cpu	**cpus;		/* possible CPUs */

	size_t n_clusters;
	struct wayca_cluster	**ccls;		/* array of clusters */

	size_t n_nodes;
	cpu_set_t *node_map;
	struct wayca_node	**nodes;	/* array of numa nodes */

	size_t n_packages;
	struct wayca_package	**packages;	/* array of Pacakges */
};

#endif /* _TOPO_H */

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

#define WAYCA_SC_SYSDEV_FNAME 	"/sys/devices"
#define WAYCA_SC_NODE_FNAME 	"/sys/devices/system/node"
#define WAYCA_SC_CPU_FNAME 	"/sys/devices/system/cpu"

#define WAYCA_SC_DEFAULT_KERNEL_MAX 	(2048)
#define WAYCA_SC_PATH_LEN_MAX		(4096)		/* maximum length of file pathname */
#define WAYCA_SC_MAX_FD_RETRIES		(5)		/* maximum retries when reading from an open file */
#define WAYCA_SC_USLEEP_DELAY_250MS	(250000)	/* 250ms */
#define WAYCA_SC_ATTR_STRING_LEN	(256)		/* default attribute string length */

#define PRINT_DBG	printf
#define PRINT_ERROR	printf

struct wayca_cache {
	int id;
	int level;
	char type[WAYCA_SC_ATTR_STRING_LEN];
	char allocation_policy[WAYCA_SC_ATTR_STRING_LEN];
	char write_policy[WAYCA_SC_ATTR_STRING_LEN];
	char cache_size[WAYCA_SC_ATTR_STRING_LEN];

	unsigned int	ways_of_associativity;
	unsigned int	physical_line_partition;
	unsigned int	number_of_sets;
	unsigned int	coherency_line_size;

	cpu_set_t *shared_cpu_map;
};

struct wayca_cpu {
	int cpu_id;
	int core_id;
	struct wayca_cluster	*p_cluster;	/* in which cluster */
	struct wayca_node	*p_numa_node;	/* in which Numa node */
	struct wayca_package	*p_package;	/* in which Package */
	cpu_set_t *core_cpus_map;		/* SMT - simultaneous multi-threading siblings; CPUs within the same core
						 *   (deprecated name: "thread_siblings_list"
						 */
	size_t n_caches;			/* number of caches */
	struct wayca_cache	*p_caches;	/* a matrix with n_caches entries */
};

struct wayca_cluster {
	int cluster_id;
	size_t n_cpus;		/* number of CPUs in this cluster */
	cpu_set_t *cpu_map;	/* mask of contained CPUs */
};

struct wayca_irq {
	unsigned int irq_number;
	unsigned int active; 		/* actively used in /proc/interrupts
					 * 1: active;
					 * 0: inactive;
					 */
	/* TODO: fine tune irq_name space */
	char irq_name[WAYCA_SC_ATTR_STRING_LEN];	/* string as reported in /proc/interrupts */
};

struct wayca_device_irqs {
	size_t n_irqs;		/* number of irqs for this device */
	struct wayca_irq *irqs; /* array */
};

struct wayca_pci_device {
	int numa_node;
	char absolute_path[WAYCA_SC_PATH_LEN_MAX];
	cpu_set_t *local_cpu_map;

	unsigned int   class;		/* 3 bytes: (base, sub, prog-if) */
	unsigned short vendor;
	unsigned short device;
	unsigned int enable;

	struct wayca_device_irqs irqs;	/* array of registered irqs */
};

struct wayca_node {
	int node_idx;			/* index of node */
	size_t n_cpus;			/* number of CPUs in this numa node */
	cpu_set_t *cpu_map;		/* mask of contained CPUs */
	cpu_set_t *cluster_map;		/* mask of contained clusters */

	int *distance;			/* array of distance */
	struct wayca_meminfo	*p_meminfo;	/* memory information of this node */

	size_t n_pcidevs;			/* number of detected PCI devices */
	struct wayca_pci_device **pcidevs;	/* array of PCI devices */
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

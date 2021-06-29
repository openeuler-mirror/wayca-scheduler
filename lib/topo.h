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

/* CPU - a logical Linux CPU (aka. a thread) which is a single scheduling unit.
 */
struct wayca_cpu {
	int cpu_id;
	int core_id;				/* to which core it belongs to */
	struct wayca_cluster	*p_cluster;	/* in which cluster */
	struct wayca_node	*p_numa_node;	/* in which Numa node */
	struct wayca_package	*p_package;	/* in which Package */
	cpu_set_t *core_cpus_map;		/* SMT - simultaneous multi-threading siblings; CPUs within the same core
						 *   (deprecated name: "thread_siblings_list"
						 */
	size_t n_caches;			/* number of caches */
	struct wayca_cache	*p_caches;	/* a matrix with n_caches entries */
};

/* Core - A core consists of 1 or more Linux CPUs (i.e. threads)
 */
struct wayca_core {
	int core_id;				/* unique ID to label a core */
	size_t n_cpus;				/* number of CPUs contained by this core */
	cpu_set_t *core_cpus_map;		/* which CPUs it contains */

	struct wayca_cluster	*p_cluster;	/* in which cluster */
	struct wayca_node	*p_numa_node;	/* in which Numa node */
	struct wayca_package	*p_package;	/* in which Package */

	size_t n_caches;			/* number of caches */
	struct wayca_cache	*p_caches;	/* a matrix with n_caches entries */
};

struct wayca_cluster {
	int cluster_id;
	size_t n_cpus;		/* number of CPUs in this cluster */
	cpu_set_t *cpu_map;	/* mask of contained CPUs */
};

struct wayca_smmu {
	int smmu_idx;				/* index, a sequence number from 0 to ... */
	int numa_node;				/* which node it belongs to */
	unsigned long long int base_addr;	/* base address - 64 bits */
	char modalias[WAYCA_SC_ATTR_STRING_LEN];	/* type, eg. arm-smmu-v3 */
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
	int numa_node;			/* to which numa_node it belongs */
	int smmu_idx;			/* to which smmu it belongs. -1 none */
	int enable;
	char absolute_path[WAYCA_SC_PATH_LEN_MAX];
	cpu_set_t *local_cpu_map;

	unsigned int   class;		/* 3 bytes: (base, sub, prog-if) */
	unsigned short vendor;
	unsigned short device;

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

	size_t n_smmus;			/* number of detected SMMU devices */
	struct wayca_smmu **smmus;	/* array of SMMU devices */
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

	size_t n_cores;				/* number of cores in this node */
	struct wayca_core **cores;		/* array of cores */

	size_t n_clusters;
	struct wayca_cluster	**ccls;		/* array of clusters */

	size_t n_nodes;
	cpu_set_t *node_map;
	struct wayca_node	**nodes;	/* array of numa nodes */

	size_t n_packages;
	struct wayca_package	**packages;	/* array of Pacakges */
};

#endif /* _TOPO_H */

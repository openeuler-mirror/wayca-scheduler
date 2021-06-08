/* topo.c
 * Author: Guodong Xu <guodong.xu@linaro.org>
 * License: GPLv2
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "common.h"
#include "topo.h"

static struct wayca_topo topo;

/* return -1 on error, 0 on success */
static int topo_path_read_s32(const char *base, const char *filename, int *result)
{
	int dir_fd;
	int fd;
	FILE *f;
	int ret, t;

	dir_fd = open(base, O_RDONLY|O_CLOEXEC);
	if (dir_fd == -1) return -1;

	fd = openat(dir_fd, filename, O_RDONLY);
	if (fd == -1) {
		close(dir_fd);
		return -1;
	}

	f = fdopen(fd, "r");
	if (f == NULL) {
		close(fd);
		close(dir_fd);
		return -1;
	}

	ret = fscanf(f, "%d", &t);

	/* close */
	fclose(f);
	close(fd);
	close(dir_fd);

	/* return */
	if (ret != 1)
		return -1;
	if (result)
		*result = t;
	return 0;
}

/* topo_path_read_multi_s32 - read nmemb s32 integer from the given filename
 *  - array: is a pre-allocated integer array
 *  - nmemb: number of members to read from file "base/filename"
 *
 * return -1 on error, 0 on success */
static int topo_path_read_multi_s32(const char *base, const char *filename, size_t nmemb, int array[])
{
	int dir_fd;
	int fd;
	FILE *f;
	int i, t;
	int ret = 0;

	dir_fd = open(base, O_RDONLY|O_CLOEXEC);
	if (dir_fd == -1) return -1;

	fd = openat(dir_fd, filename, O_RDONLY);
	if (fd == -1) {
		close(dir_fd);
		return -1;
	}

	f = fdopen(fd, "r");
	if (f == NULL) {
		close(fd);
		close(dir_fd);
		return -1;
	}

	for (i = 0; i < nmemb; i++) {
		if (fscanf(f, "%d", &t) != 1) {
			ret = -1;	/* on failure */
			break;
		}
		array[i] = t;		/* on success */
	}

	/* close */
	fclose(f);
	close(fd);
	close(dir_fd);

	/* return */
	return ret;
}

/* topo_path_parse_meminfo - parse 'meminfo'
 *  - filename: usually, this is 'meminfo'
 *  - p_meminfo: a pre-allocated space to store parsing results
 *
 * return -1 on error, 0 on success */
static int topo_path_parse_meminfo(const char *base, const char *filename, struct wayca_meminfo *p_meminfo)
{
	int dir_fd;
	int fd;
	FILE *f;
	char buf[BUFSIZ];		/* BUFSIZ defined in GCC, 8kbytes */
	char *ptr;
	int ret = -1;

	dir_fd = open(base, O_RDONLY|O_CLOEXEC);
	if (dir_fd == -1) return -1;

	fd = openat(dir_fd, filename, O_RDONLY);
	if (fd == -1) {
		close(dir_fd);
		return -1;
	}

	f = fdopen(fd, "r");
	if (f == NULL) {
		close(fd);
		close(dir_fd);
		return -1;
	}

	/* the real work */
	while (fgets(buf, sizeof(buf), f) != NULL) {
		/* search the string */
		if ((ptr = strstr(buf, "MemTotal:")) == NULL)	/* not found */
			continue;
		/* read in kB */
		/* Note: format here is copied from [kernel]/drivers/base/node.c */
		if (sscanf(ptr, "%*s %8lu", &p_meminfo->total_avail_kB) != 1)
			break;	/* failed to parse*/
		ret = 0;
		break;  /* on success, break early */
	}

	/* close */
	fclose(f);
	close(fd);
	close(dir_fd);

	/* return */
	return ret;
}



/* Note: cpuset_nbits(), nextnumber(), nexttoken(), cpulist_parse() are referenced
 *       from https://github.com/karelzak/util-linux
 */
#define cpuset_nbits(setsize)	(8 * (setsize))

static const char *nexttoken(const char *q,  int sep)
{
	if (q)
		q = strchr(q, sep);
	if (q)
		q++;
	return q;
}

static int nextnumber(const char *str, char **end, unsigned int *result)
{
	errno = 0;
	if (str == NULL || *str == '\0' || !isdigit(*str))
		return -EINVAL;
	*result = (unsigned int) strtoul(str, end, 10);
	if (errno)
		return -errno;
	if (str == *end)
		return -EINVAL;
	return 0;
}

/*
 * Parses string with list of CPU ranges.
 * Returns 0 on success.
 * Returns 1 on error.
 * Returns 2 if fail is set and a cpu number passed in the list doesn't fit
 * into the cpu_set. If fail is not set cpu numbers that do not fit are
 * ignored and 0 is returned instead.
 */
int cpulist_parse(const char *str, cpu_set_t *set, size_t setsize, int fail)
{
	size_t max = cpuset_nbits(setsize);
	const char *p, *q;
	char *end = NULL;

	q = str;
	CPU_ZERO_S(setsize, set);

	while (p = q, q = nexttoken(q, ','), p) {
		unsigned int a;	/* beginning of range */
		unsigned int b;	/* end of range */
		unsigned int s;	/* stride */
		const char *c1, *c2;

		if (nextnumber(p, &end, &a) != 0)
			return 1;
		b = a;
		s = 1;
		p = end;

		c1 = nexttoken(p, '-');
		c2 = nexttoken(p, ',');

		if (c1 != NULL && (c2 == NULL || c1 < c2)) {
			if (nextnumber(c1, &end, &b) != 0)
				return 1;

			c1 = end && *end ? nexttoken(end, ':') : NULL;

			if (c1 != NULL && (c2 == NULL || c1 < c2)) {
				if (nextnumber(c1, &end, &s) != 0)
					return 1;
				if (s == 0)
					return 1;
			}
		}

		if (!(a <= b))
			return 1;
		while (a <= b) {
			if (fail && (a >= max))
				return 2;
			CPU_SET_S(a, setsize, set);
			a += s;
		}
	}

	if (end && *end)
		return 1;
	return 0;
}

/* return negative value on error, 0 on success */
/* cpu_set_t *set: can contain anything and will be zero'ed by cpulist_parse() */
static int topo_path_read_cpulist(const char *base, const char *filename, cpu_set_t *set, int maxcpus)
{
	int dir_fd = open(base, O_RDONLY|O_CLOEXEC);
	int fd = openat(dir_fd, filename, O_RDONLY);
	FILE *f = fdopen(fd, "r");

	size_t len = maxcpus * 8;	/* big enough to hold a CPU ids */
	char buf[len];			/* dynamic allocation */
	int ret = 0;

	if (fgets(buf, len, f) == NULL)
		ret = -errno;

	/* close */
	fclose(f);
	close(fd);
	close(dir_fd);

	if (ret != 0)
		return ret;

	len = strlen(buf);
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	if (cpulist_parse(buf, set, CPU_ALLOC_SIZE(maxcpus), 0))
		return -EINVAL;

	return 0;
}

/* topo_read_cpu_topology() - read cpu%d topoloy, where %d is cpu_index
 *
 * Return -1 on error, 0 on success
 */
static int topo_read_cpu_topology(int cpu_index, struct wayca_topo *p_topo)
{
	static int max_node_index = -1;		/* actual node_index starts from 0 */

	DIR *dir;
	struct dirent *dirent;
	long node_index;
	char *endptr;
	char path_buffer[PATH_LEN_MAX];

	int core_id, cluster_id, ppkg_id;
	int i;

	/* allocate a new struct wayca_cpu */
	p_topo->cpus[cpu_index] = (struct wayca_cpu *)calloc(1, sizeof(struct wayca_cpu));	/* allocated memory is set to zero */
	if (!p_topo->cpus[cpu_index]) {
		return -1;	/* no enough memory */
	}
	p_topo->cpus[cpu_index]->cpu_id = cpu_index;

	sprintf(path_buffer, "%s/cpu%d", CPU_FNAME, cpu_index);

	/* read cpu%d/node* to learn which numa node this cpu belongs to */
	dir = opendir(path_buffer);
	if (!dir)
		return -1;

	while ((dirent = readdir(dir)) != NULL) {
		if (strncmp(dirent->d_name, "node", 4))
			continue;
		/* read node_index */
		node_index = strtol(dirent->d_name+4, &endptr, 0);
		if (endptr == dirent->d_name+4) /* not a number */
			continue;
		/* check whether need more node space */
		if (node_index > max_node_index) {
			max_node_index = node_index;
			/* re-allocate for more space */
			struct wayca_node **p_temp;
			p_temp = (struct wayca_node **)realloc(p_topo->nodes,
					(max_node_index + 1) * sizeof(struct wayca_node *));
			if (!p_temp) {
				closedir(dir);
				return -1;	/* no enough memory */
			}
			p_topo->nodes = p_temp;
		}
		/* check this 'node_index' node exist or not */
		/* if not, create one
		 * TODO: refactor wayca_node_create into a new function */
		if (!CPU_ISSET_S(node_index, CPU_ALLOC_SIZE(p_topo->n_cpus), p_topo->node_map)) {
			p_topo->nodes[node_index] = (struct wayca_node *)calloc(1, sizeof(struct wayca_node));	/* allocated memory is set to zero */
			if (!p_topo->nodes[node_index]) {
				closedir(dir);
				return -1;	/* no enough memory */
			}
			p_topo->nodes[node_index]->node_idx = node_index;
			/* initialize this node's cpu_map */
			p_topo->nodes[node_index]->cpu_map = CPU_ALLOC(p_topo->kernel_max_cpus);
			if (!p_topo->nodes[node_index]->cpu_map) {
				closedir(dir);
				return -1;			/* no enough memory */
			}
			CPU_ZERO_S(p_topo->setsize, p_topo->nodes[node_index]->cpu_map);
			/* add node_index into the top-level node map */
			CPU_SET_S(node_index, CPU_ALLOC_SIZE(p_topo->n_cpus), p_topo->node_map);
			p_topo->n_nodes ++;
		}
		/* add current CPU into this node's cpu map */
		CPU_SET_S(cpu_index, p_topo->setsize, p_topo->nodes[node_index]->cpu_map);
		p_topo->nodes[node_index]->n_cpus ++;
		/* link this node back to current CPU */
		p_topo->cpus[cpu_index]->p_numa_node = p_topo->nodes[node_index];
		break;	/* found one "node" entry, no need to check any more */
	}
	closedir(dir);

	/* move the base to "cpu%d/topology" */
	sprintf(path_buffer, "%s/cpu%d/topology", CPU_FNAME, cpu_index);

	/* read "core_id" */
	if (topo_path_read_s32(path_buffer, "core_id", &core_id) != 0)	/* on failure */
		p_topo->cpus[cpu_index]->core_id = -1;
	p_topo->cpus[cpu_index]->core_id = core_id;	/* on success */

	/* read "cluster_id" */
	if (topo_path_read_s32(path_buffer, "cluster_id", &cluster_id) != 0) {	/* on failure */
		p_topo->cpus[cpu_index]->p_cluster = NULL;
		goto read_package_info;
	}

	/* check this "cluster_id" exists or not */
	for (i = 0; i < p_topo->n_clusters; i++) {
		if (p_topo->ccls[i]->cluster_id == cluster_id)
			break;
	}
	if (i == p_topo->n_clusters) {	/* need to create a new wayca_cluster */
		/* TODO: refactor this into a new funcion */
		p_topo->n_clusters ++;
		/* increase p_topo->ccls array */
		struct wayca_cluster **p_temp;
		p_temp = (struct wayca_cluster **)realloc(p_topo->ccls, (p_topo->n_clusters) * sizeof(struct wayca_cluster *));
		if (!p_temp)
			return -1;	/* no enough memory */
		p_topo->ccls = p_temp;
		/* allocate a new wayca_cluster struct, and link it to p_topo->ccls */
		p_topo->ccls[i] = (struct wayca_cluster *)calloc(1, sizeof(struct wayca_cluster));	/* with calloc, allocated memory is set to zero */
		if (!p_topo->ccls[i])
			return -1;	/* no enough memory */
		/* initialize this cluster's cpu_map */
		p_topo->ccls[i]->cpu_map = CPU_ALLOC(p_topo->kernel_max_cpus);
		if (!p_topo->ccls[i]->cpu_map)
			return -1;			/* no enough memory */
		/* read "cluster_cpus_list" */
		if (topo_path_read_cpulist(path_buffer, "cluster_cpus_list",
					   p_topo->ccls[i]->cpu_map, p_topo->kernel_max_cpus) != 0)
			return -1;	/* failed */
		/* assign cluster_id and n_cpus */
		p_topo->ccls[i]->cluster_id = cluster_id;
		p_topo->ccls[i]->n_cpus = CPU_COUNT_S(p_topo->setsize, p_topo->ccls[i]->cpu_map);
	}
	/* link this cluster back to current CPU */
	p_topo->cpus[cpu_index]->p_cluster = p_topo->ccls[i];

read_package_info:
	/* read "physical_package_id" */
	if (topo_path_read_s32(path_buffer, "physical_package_id", &ppkg_id) != 0)	/* on failure */
		return -1;

	/* check this "physical_package_id" exists or not */
	for (i = 0; i < p_topo->n_packages; i++)
		if (p_topo->packages[i]->physical_package_id == ppkg_id)
			break;
	if (i == p_topo->n_packages) {	/* need to create a new wayca_package */
		/* TODO: refactor this into a new funcion */
		p_topo->n_packages ++;
		/* increase p_topo->packages array */
		struct wayca_package **p_temp;
		p_temp = (struct wayca_package **)realloc(p_topo->packages,
			   p_topo->n_packages * sizeof(struct wayca_package *));
		if (!p_temp)
			return -1;	/* no enough memory */
		p_topo->packages = p_temp;
		/* allocate a new wayca_ package struct, and link it to p_topo->ccls */
		p_topo->packages[i] = (struct wayca_package *)calloc(1, sizeof(struct wayca_package));	/* with calloc, allocated memory is set to zero */
		if (!p_topo->packages[i])
			return -1;	/* no enough memory */
		/* initialize this package's cpu_map */
		p_topo->packages[i]->cpu_map = CPU_ALLOC(p_topo->kernel_max_cpus);
		if (!p_topo->packages[i]->cpu_map)
			return -1;			/* no enough memory */
		/* read "package_cpus_list" */
		if (topo_path_read_cpulist(path_buffer, "package_cpus_list",
				p_topo->packages[i]->cpu_map, p_topo->kernel_max_cpus) != 0)
			return -1;	/* failed */
		/* assign physical_package_id and n_cpus */
		p_topo->packages[i]->physical_package_id = ppkg_id;
		p_topo->packages[i]->n_cpus = CPU_COUNT_S(p_topo->setsize,
							  p_topo->packages[i]->cpu_map);
	}
	/* link this package back to current CPU */
	p_topo->cpus[cpu_index]->p_package = p_topo->packages[i];

	/* read core_cpus_list, (SMT: simultaneous multi-threading) */
	p_topo->cpus[cpu_index]->core_cpus_map = CPU_ALLOC(p_topo->kernel_max_cpus);
	if (!p_topo->cpus[cpu_index]->core_cpus_map)
		return -1;			/* no enough memory */
	/* read "core_cpus_list" */
	if (topo_path_read_cpulist(path_buffer, "core_cpus_list",
				   p_topo->cpus[cpu_index]->core_cpus_map,
				   p_topo->kernel_max_cpus) != 0)
		return -1;	/* failed */

	return 0;	/* on success */
}

/* topo_read_node_topology() - read node%d topoloy, where %d is node_index
 *
 * Return -1 on error, 0 on success
 */
static int topo_read_node_topology(int node_index, struct wayca_topo *p_topo)
{
	cpu_set_t *node_cpu_map;
	char path_buffer[PATH_LEN_MAX];
	int *distance_array;

	sprintf(path_buffer, "%s/node%d", NODE_FNAME, node_index);

	/* read node's cpulist */
	node_cpu_map = CPU_ALLOC(p_topo->kernel_max_cpus);
	if (!node_cpu_map)
		return -ENOMEM;			/* nothing to clean up, just return */
	if (topo_path_read_cpulist(path_buffer, "cpulist",
			node_cpu_map, p_topo->kernel_max_cpus) != 0)
		return -1;	/* failed */
	/* check w/ what's previously composed in cpu_topology reading */
	if (!CPU_EQUAL_S(p_topo->setsize, node_cpu_map, p_topo->nodes[node_index]->cpu_map)) {
		PRINT_ERROR("mismatch detected in node%d cpulist read", node_index);
		return -1;
	}
	CPU_FREE(node_cpu_map);

	/* allocate a distance array*/
	distance_array = (int *)calloc(p_topo->n_nodes, sizeof(int));
	if (!distance_array)
		return -ENOMEM;
	/* read node's distance */
	if (topo_path_read_multi_s32(path_buffer, "distance", p_topo->n_nodes, distance_array) != 0) {
		free(distance_array);
		return -1; 	/* failed */
	}
	/* assign */
	p_topo->nodes[node_index]->distance = distance_array;

	/* read meminfo */
	/* allocate a struct */
	struct wayca_meminfo *meminfo_tmp = (struct wayca_meminfo *)calloc(1, sizeof(struct wayca_meminfo));
	if (!meminfo_tmp)
		return -ENOMEM;
	if (topo_path_parse_meminfo(path_buffer, "meminfo", meminfo_tmp) != 0) {
		free(meminfo_tmp);
		return -1; 	/* failed */
	}
	/* assign */
	p_topo->nodes[node_index]->p_meminfo = meminfo_tmp;

	return 0;
}

/* External callable functions */

void __attribute__ ((constructor)) topo_init(void);

void topo_init(void)
{
	struct wayca_topo *p_topo = &topo;
	cpu_set_t *cpuset_possible;
	cpu_set_t *node_possible;
	int i = 0;

	memset(p_topo, 0, sizeof(struct wayca_topo));

	/* read "cpu/kernel_max" to determine maximum size for future memory allocations */
	if (topo_path_read_s32(CPU_FNAME, "kernel_max", &p_topo->kernel_max_cpus) == 0)
		p_topo->kernel_max_cpus += 1;
	else
		p_topo->kernel_max_cpus = DEFAULT_KERNEL_MAX;
	p_topo->setsize = CPU_ALLOC_SIZE(p_topo->kernel_max_cpus);

	/* read "cpu/possible" to determine number of CPUs */
	cpuset_possible = CPU_ALLOC(p_topo->kernel_max_cpus);
	if (!cpuset_possible)
		return;			/* nothing to clean up, just return */

	if (topo_path_read_cpulist(CPU_FNAME, "possible", cpuset_possible, p_topo->kernel_max_cpus) == 0) {
		/* determine number of CPUs in cpuset_possible */
		p_topo->n_cpus = CPU_COUNT_S(p_topo->setsize, cpuset_possible);
		p_topo->cpu_map = cpuset_possible;
		/* allocate wayca_cpu for each CPU */
		p_topo->cpus = (struct wayca_cpu **)calloc(p_topo->n_cpus, sizeof(struct wayca_cpu *));
		/* TODO: check for calloc failure */
	} else {
		PRINT_ERROR("failed to read possible CPUs");
		goto cleanup_on_error;
	}

	/* initialize node_map */
	p_topo->node_map = CPU_ALLOC(p_topo->n_cpus);
	if (!p_topo->node_map)
		goto cleanup_on_error;
	CPU_ZERO_S(CPU_ALLOC_SIZE(p_topo->n_cpus), p_topo->node_map);

	/* read all cpu%d topology */
	for (i = 0; i < p_topo->n_cpus; i++) {
		if (topo_read_cpu_topology(i, p_topo) == -1) {         /* on failure */
			goto cleanup_on_error;
		}
	}
	/* at the end of the cpu%d for loop, the following has been established:
	 *  - p_topo->n_nodes
	 *  - p_topo->node_map
	 *  - p_topo->nodes[]
	 *  - p_topo->n_clusters
	 *  - p_topo->ccls[]
	 *  - p_topo->n_packages
	 *  - p_topo->packages[]
	 */

	node_possible = CPU_ALLOC(p_topo->n_cpus);
	if (!node_possible)
		goto cleanup_on_error;

	/* read "node/possible" to determine number of numa nodes */
	if (topo_path_read_cpulist(NODE_FNAME, "possible", node_possible, p_topo->n_cpus) == 0) {
		if (!CPU_EQUAL_S(CPU_ALLOC_SIZE(p_topo->n_cpus), node_possible, p_topo->node_map) ||
		    CPU_COUNT_S(CPU_ALLOC_SIZE(p_topo->n_cpus), node_possible) != p_topo->n_nodes) {
			/* mismatch with what cpu topology shows */
			PRINT_ERROR("node/possible mismatch with what cpu topology shows");
			goto cleanup_on_error;
		 }
	} else {
		PRINT_ERROR("failed to read possible NODEs");
		goto cleanup_on_error;
	}

	/* read all node%d topology */
	for (i = 0; i < p_topo->n_nodes; i++) {
		if (topo_read_node_topology(i, p_topo) == -1) /* on failure */ {
			goto cleanup_on_error;
		}
	}
	/* at the end of the cpu%d for loop, the following has been established:
	 *  - p_topo->nodes[]->distance
	 */
	return;

	/* cleanup_on_error */
cleanup_on_error:
	topo_free(p_topo);
	return;
}


/* print the topology */
void topo_print_wayca_cluster(size_t setsize, struct wayca_cluster *p_cluster)
{
	PRINT_DBG("cluster_id: %08x\n", p_cluster->cluster_id);
	PRINT_DBG("n_cpus: %lu\n", p_cluster->n_cpus);
	PRINT_DBG("\tCpu count in this cluster's cpu_map: %d\n", CPU_COUNT_S(setsize, p_cluster->cpu_map));
}

void topo_print_wayca_node(size_t setsize, struct wayca_node *p_node, size_t distance_size)
{
	PRINT_DBG("node index: %d\n", p_node->node_idx);
	PRINT_DBG("n_cpus: %lu\n", p_node->n_cpus);
	PRINT_DBG("\tCpu count in this node's cpu_map: %d\n", CPU_COUNT_S(setsize, p_node->cpu_map));
	PRINT_DBG("total memory (in kB): %8lu\n", p_node->p_meminfo->total_avail_kB);
	PRINT_DBG("distance: ");
	for (int i = 0; i < distance_size; i++)
		PRINT_DBG("%d\t", p_node->distance[i]);
	PRINT_DBG("\n");
	/* TODO: fill up the following */
	PRINT_DBG("pointer of cluster_map: 0x%p EXPECTED (nil)\n", p_node->cluster_map);
}

void topo_print_wayca_cpu(size_t setsize, struct wayca_cpu *p_cpu)
{
	PRINT_DBG("cpu_id: %d\n", p_cpu->cpu_id);
	PRINT_DBG("core_id: %d\n", p_cpu->core_id);
	PRINT_DBG("\tCPU count in this core / SMT factor: %d\n",
				CPU_COUNT_S(setsize, p_cpu->core_cpus_map));
	if (p_cpu->p_cluster != NULL)
		PRINT_DBG("belongs to cluster_id: \t%08x\n", p_cpu->p_cluster->cluster_id);
	PRINT_DBG("belongs to node: \t%d\n", p_cpu->p_numa_node->node_idx);
	PRINT_DBG("belongs to package_id: \t%08x\n", p_cpu->p_package->physical_package_id);
}

void topo_print(void)
{
	struct wayca_topo *p_topo = &topo;
	int i;

	PRINT_DBG("kernel_max_cpus: %d\n", p_topo->kernel_max_cpus);
	PRINT_DBG("setsize: %lu\n", p_topo->setsize);

	PRINT_DBG("n_cpus: %lu\n", p_topo->n_cpus);
	PRINT_DBG("\tCpu count in cpu_map: %d\n", CPU_COUNT_S(p_topo->setsize, p_topo->cpu_map));
	for (i = 0; i < p_topo->n_cpus; i++) {
		if (p_topo->cpus[i] == NULL)
			continue;
		PRINT_DBG("CPU%d information:\n", i);
		topo_print_wayca_cpu(p_topo->setsize, p_topo->cpus[i]);
	}

	PRINT_DBG("n_clusters: %lu\n", p_topo->n_clusters);
	for (i = 0; i < p_topo->n_clusters; i++) {
		if (p_topo->ccls[i] == NULL)
			continue;
		PRINT_DBG("Cluster %d information:\n", i);
		topo_print_wayca_cluster(p_topo->setsize, p_topo->ccls[i]);
	}


	PRINT_DBG("n_nodes: %lu\n", p_topo->n_nodes);
	PRINT_DBG("\tNode count in node_map: %d\n", CPU_COUNT_S(CPU_ALLOC_SIZE(p_topo->n_cpus), p_topo->node_map));
	for (i = 0; i < p_topo->n_nodes; i++) {
		if (p_topo->nodes[i] == NULL)
			continue;
		PRINT_DBG("NODE%d information:\n", i);
		topo_print_wayca_node(p_topo->setsize, p_topo->nodes[i], p_topo->n_nodes);
	}
	PRINT_DBG("n_packages: %lu\n", p_topo->n_packages);

	return;
}

void __attribute__ ((destructor)) topo_free(void);

/* topo_free - free up memories */
void topo_free(void)
{
	struct wayca_topo *p_topo = &topo;
	int i;

	CPU_FREE(p_topo->cpu_map);
	for (i = 0; i < p_topo->n_cpus; i++) {
		CPU_FREE(p_topo->cpus[i]->core_cpus_map);
		free(p_topo->cpus[i]);
	}
	free(p_topo->cpus);

	for (i = 0; i < p_topo->n_clusters; i++) {
		CPU_FREE(p_topo->ccls[i]->cpu_map);
		free(p_topo->cpus[i]);
	}
	free(p_topo->ccls);

	CPU_FREE(p_topo->node_map);
	for (i = 0; i < p_topo->n_nodes; i++) {
		CPU_FREE(p_topo->nodes[i]->cpu_map);
		CPU_FREE(p_topo->nodes[i]->cluster_map);
		free(p_topo->nodes[i]->distance);
		free(p_topo->nodes[i]->p_meminfo);
		free(p_topo->nodes[i]);
	}
	free(p_topo->nodes);

	for (i = 0; i < p_topo->n_packages; i++) {
		CPU_FREE(p_topo->packages[i]->cpu_map);
		CPU_FREE(p_topo->packages[i]->numa_map);
		free(p_topo->packages[i]);
	}
	free(p_topo->packages);

	memset(p_topo, 0, sizeof(struct wayca_topo));
	return;
}

int cores_in_ccl(void)
{
	if (topo.n_clusters < 1)
		return -1;	/* not initialized */
	return topo.ccls[0]->n_cpus;
}

int cores_in_node(void)
{
	if (topo.n_nodes < 1)
		return -1;	/* not initialized */
	return topo.nodes[0]->n_cpus;
}

int cores_in_package(void)
{
	if (topo.n_packages < 1)
		return -1;	/* not initialized */
	return topo.packages[0]->n_cpus;
}

int cores_in_total(void)
{
	if (topo.n_cpus < 1)
		return -1;	/* not initialized */
	return topo.n_cpus;
}

int nodes_in_package(void)
{
	if (topo.n_packages < 1)
		return -1;	/* not initialized */
	return topo.packages[0]->n_cpus / topo.nodes[0]->n_cpus;
}

int nodes_in_total(void)
{
	if (topo.n_nodes < 1)
		return -1;	/* not initialized */
	return topo.n_nodes;
}

/* memory bandwidth (relative value) of speading over multiple CCLs
 *
 * Measured with: bw_mem bcopy
 *
 * Implication:
 *  6 CCLs (clusters) per NUMA node
 *  multiple threads run spreadingly over multiple Clusters. One thread per core.
 */
int mem_bandwidth_6CCL[][6] = {
	/* 1CCL, 2CCLs, 3CCLs, 4CCLs, 5CCLs, 6CCLs */
	{  15,   18,    18,    19,    19,    19},  /* 4 threads */
	{  0,    23,    23,    24,    24,    24},  /* 8 threads */
	{  0,    0,     28,    28,    28,    29},  /* 12 threads */
	{  0,    0,     0,     31,    32,    31},  /* 16 threads */
};

/* memory bandwidth (relative value) of speading over multiple NUMA nodes
 *
 * Measured with: bw_mem bcopy
 *
 * Implication:
 *  4 NUMA nodes
 *  multiple threads run spreadingly over multiple NUMA nodes. One thread per core.
 */
int mem_bandwidth_4NUMA[][4] = {
	/* 1NUMA, 2NUMA, 3NUMA, 4NUMA */
	{  33,    55,    68,    79 }, /* 24 threads */
	{  0,     66,    92,    112}, /* 48 threads */
	{  0,     0,     99,    130}  /* 72 threads */
};

/* memory bandwidth when computing is on one NUMA, but memory is interleaved on different NUMA node(s)
 * Measured with: numactl --interleave, bw_mem bcopy
 *
 * Implication:
 *  4 NUMA nodes
 *  multiple threads run spreadingly over multiple NUMA nodes. One thread per core.
 */
int mem_bandwidth_interleave_4NUMA[][7] = {
	/* Same | Neighbor | Remote | Remote | Neighbor |         |         *
	 * NUMA |  NUMA    | NUMA0  | NUMA1  |  2 NUMAs | 3 NUMAs | 4 NUMAs */
	{  19,    5,         9,       6,       9,         11,       9 },    /* 4 threads */
	{  24,    5,         7,       6,       10,        14,       13},    /* 8 threads */
	{  29,    5,         7,       6,       10,        15,       13},    /* 12 threads */
	{  31,    5,         7,       6,       10,        16,       13}     /* 16 threads */
};

/* memory read latency for range [1M ~ 8M], multiple threads spreading over multiple CCLs, same NUMA
 *
 * Measured with: lat_mem_rd
 * Implication:
 *  6 CCLs (clusters) per NUMA node
 *  multiple threads run spreadingly over multiple Clusters. One thread per core.
 */
int mem_rd_latency_1M_6CCL[][6] = {
	/* 1CCL, 2CCLs, 3CCLs, 4CCLs, 5CCLs, 6CCLs */
	{  13,   6,     4,     4,     4,     4 },  /* 4 threads */
	{  0,    12,    6,     9,     5,     5 },  /* 8 threads */
	{  0,    0,     16,    15,    12,    10},  /* 12 threads */
	{  0,    0,     0,     17,    14,    12},  /* 16 threads */
};

/* memory read latency for range [12M ~ 2G+], multiple threads spreading over multiple CCLs, same NUMA
 *
 * Measured with: lat_mem_rd
 * Implication:
 *  6 CCLs (clusters) per NUMA node
 *  multiple threads run spreadingly over multiple Clusters. One thread per core.
 */
int mem_rd_latency_12M_6CCL[][6] = {
	/* 1CCL, 2CCLs, 3CCLs, 4CCLs, 5CCLs, 6CCLs */
	{  13,   8,     6,     6,     6,     6 },  /* 4 threads */
	{  0,    14,    9,     9,     8,     8 },  /* 8 threads */
	{  0,    0,     15,    12,    11,    11},  /* 12 threads */
	{  0,    0,     0,     16,    14,    13},  /* 16 threads */
};

/* memory read latency for range [1M ~ 8M], multiple threads spreading over multiple NUMAs
 *
 * Measured with: lat_mem_rd
 * Implication:
 *  4 NUMA nodes
 *  multiple threads run spreadingly over multiple NUMAs. One thread per core.
 */
int mem_rd_latency_1M_4NUMA[][4] = {
	/* 1NUMA, 2NUMA, 3NUMA, 4NUMA */
	{  19,    16,    11,    6  }, /* 24 threads */
	{  0,     19,    17,    14 }, /* 48 threads */
	{  0,     0,     17,    9  }  /* 72 threads */
};

/* memory read latency for range [12M ~ 2G+], multiple threads spreading over multiple NUMAs
 *
 * Measured with: lat_mem_rd
 * Implication:
 *  4 NUMA nodes
 *  multiple threads run spreadingly over multiple NUMAs. One thread per core.
 */
int mem_rd_latency_12M_4NUMA[][4] = {
	/* 1NUMA, 2NUMA, 3NUMA, 4NUMA */
	{  21,    15,    14,    8  }, /* 24 threads */
	{  0,     20,    16,    15 }, /* 48 threads */
	{  0,     0,     18,    12 }  /* 72 threads */
};

/* pipe latency within the same CCL, and across two CCLs of the same NUMA
 *
 * Measaured with: lat_pipe
 * Implications:
 *  6 CCLs (clusters) per NUMA node
 *  Pipe latencies between different CCLs have no notice-worthy difference. Just categorized
 *     'cross CCLs' in below.
 */
int pipe_latency_CCL[3] = {
	/* same | same | cross *
	 * CPU  | CCL  | CCLs  */
	   46,    49,    66   /* 2 processes in pipe communitcaiton */
};

/* pipe latency across NUMA nodes */
int pipe_latency_NUMA[4] = {
	/* Same NUMA | Neighbor | Remote | Remote *
	 * diff CCLs |  NUMAs   | NUMA0  | NUMA1  */
};


/* TODO: for recursively searching the directories for PCI devices' and finding 'numa_node' */

#if 0
void printdir(char *dir, int depth)
{
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    if((dp = opendir(dir)) == NULL) {
        fprintf(stderr,"cannot open directory: %s\n", dir);
        return;
    }
    chdir(dir);
    while((entry = readdir(dp)) != NULL) {
        lstat(entry->d_name,&statbuf);
        if(S_ISDIR(statbuf.st_mode)) {
            /* Found a directory, but ignore . and .. */
            if(strcmp(".",entry->d_name) == 0 ||
                strcmp("..",entry->d_name) == 0)
                continue;
            /* TODO: insert PCI device information retrieving code here. And insert
	     *     - insert PCI node creation code
	     */
            printf("%*s%s/\n",depth,"",entry->d_name);
            /* Recurse at a new indent level */
            printdir(entry->d_name,depth+4);
        }
        else printf("%*s%s\n",depth,"",entry->d_name);
    }
    chdir("..");
    closedir(dp);
}

int main()
{
    printf("Directory scan of /home:\n");
    printdir("/home",0);
    printf("done.\n");
    exit(0);
}

#endif
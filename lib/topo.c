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

/* topo.c
 * Author: Guodong Xu <guodong.xu@linaro.org>
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include "common.h"
#include "topo.h"
#include "log.h"
#include "wayca-scheduler.h"

WAYCA_SC_INIT_PRIO(topo_init, TOPO);
WAYCA_SC_FINI_PRIO(topo_free, TOPO);
static struct wayca_topo topo;

/* topo_path_read_buffer - read from filename into buf, maximum 'count' in size
 * return:
 *   negative on error
 *   0 or more: total number of bytes read
 */
static int topo_path_read_buffer(const char *base, const char *filename,
				 char *buf, size_t count)
{
	char *real_path = NULL;
	int c = 0, tries = 0;
	int dir_fd;
	int fd;
	FILE *f;
	int ret;

	real_path = realpath(base, NULL);
	if (!real_path)
		return -errno;

	dir_fd = open(real_path, O_RDONLY | O_CLOEXEC);
	free(real_path);
	if (dir_fd == -1)
		return -errno;

	fd = openat(dir_fd, filename, O_RDONLY);
	if (fd == -1) {
		ret = errno;
		close(dir_fd);
		return -ret;
	}

	f = fdopen(fd, "r");
	if (f == NULL) {
		ret = errno;
		close(fd);
		close(dir_fd);
		return -ret;
	}

	memset(buf, 0, count);
	while (count > 0) {
		ret = read(fd, buf, count);
		if (ret < 0) {
			if ((errno == EAGAIN || errno == EINTR) &&
			    (tries++ < WAYCA_SC_MAX_FD_RETRIES)) {
				usleep(WAYCA_SC_USLEEP_DELAY_250MS);
				continue;
			}
			c = c ? c : (-errno);
			break;
		}
		if (ret == 0)
			break;
		tries = 0;
		count -= ret;
		buf += ret;
		c += ret;
	}

	fclose(f);
	close(fd);
	close(dir_fd);

	return c;
}

/* return negative on error, 0 on success */
static int topo_path_read_s32(const char *base, const char *filename,
			      int *result)
{
	char *real_path = NULL;
	int dir_fd;
	int fd;
	FILE *f;
	int ret, t;

	real_path = realpath(base, NULL);
	if (!real_path)
		return -errno;

	dir_fd = open(real_path, O_RDONLY | O_CLOEXEC);
	free(real_path);
	if (dir_fd == -1)
		return -errno;

	fd = openat(dir_fd, filename, O_RDONLY);
	if (fd == -1) {
		ret = errno;
		close(dir_fd);
		return -ret;
	}

	f = fdopen(fd, "r");
	if (f == NULL) {
		ret = errno;
		close(fd);
		close(dir_fd);
		return -ret;
	}

	ret = fscanf(f, "%d", &t);

	fclose(f);
	close(fd);
	close(dir_fd);

	if (ret != 1)
		return -EINVAL;
	if (result)
		*result = t;
	return 0;
}

/*
 * topo_path_read_multi_s32 - read nmemb s32 integer from the given filename
 *  - array: is a pre-allocated integer array
 *  - nmemb: number of members to read from file "base/filename"
 *
 * return negative on error, 0 on success
 */
static int topo_path_read_multi_s32(const char *base, const char *filename,
				    size_t nmemb, int array[])
{
	char *real_path = NULL;
	int dir_fd;
	int fd;
	FILE *f;
	int i, t;
	int ret = 0;

	real_path = realpath(base, NULL);
	if (!real_path)
		return -errno;

	dir_fd = open(real_path, O_RDONLY | O_CLOEXEC);
	free(real_path);
	if (dir_fd == -1)
		return -errno;

	fd = openat(dir_fd, filename, O_RDONLY);
	if (fd == -1) {
		ret = errno;
		close(dir_fd);
		return -ret;
	}

	f = fdopen(fd, "r");
	if (f == NULL) {
		ret = errno;
		close(fd);
		close(dir_fd);
		return -ret;
	}

	for (i = 0; i < nmemb; i++) {
		if (fscanf(f, "%d", &t) != 1) {
			ret = -EINVAL;
			break;
		}
		array[i] = t;
	}

	fclose(f);
	close(fd);
	close(dir_fd);

	return ret;
}

/*
 * topo_path_parse_meminfo - parse 'meminfo'
 *  - filename: usually, this is 'meminfo'
 *  - p_meminfo: a pre-allocated space to store parsing results
 *
 * return negative on error, 0 on success
 */
static int topo_path_parse_meminfo(struct wayca_meminfo *p_meminfo,
					const char *base, const char *filename)
{
	char *real_path = NULL;
	char buf[BUFSIZ];
	int ret = -1;
	int dir_fd;
	char *ptr;
	FILE *f;
	int fd;

	real_path = realpath(base, NULL);
	if (!real_path)
		return -errno;

	dir_fd = open(real_path, O_RDONLY | O_CLOEXEC);
	free(real_path);
	if (dir_fd == -1)
		return -errno;

	fd = openat(dir_fd, filename, O_RDONLY);
	if (fd == -1) {
		ret = errno;
		close(dir_fd);
		return -ret;
	}

	f = fdopen(fd, "r");
	if (f == NULL) {
		ret = errno;
		close(fd);
		close(dir_fd);
		return -ret;
	}

	while (fgets(buf, sizeof(buf), f) != NULL) {
		ptr = strstr(buf, "MemTotal:");
		if (ptr == NULL)
			continue;

		if (sscanf(ptr, "%*s %lu", &p_meminfo->total_avail_kB) != 1)
			break;
		ret = 0;
		break;
	}

	fclose(f);
	close(fd);
	close(dir_fd);

	return ret;
}

/* Note: cpuset_nbits(), nextnumber(), nexttoken(), cpulist_parse() are referenced
 *       from https://github.com/karelzak/util-linux
 */
#define cpuset_nbits(setsize) (8 * (setsize))

static const char *nexttoken(const char *q, int sep)
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
	*result = (unsigned int)strtoul(str, end, 10);
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
		unsigned int a; /* beginning of range */
		unsigned int b; /* end of range */
		unsigned int s; /* stride */
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
static int topo_path_read_cpulist(const char *base, const char *filename,
				  cpu_set_t *set, int maxcpus)
{
	size_t len = maxcpus * 8; /* big enough to hold a CPU ids */
	char *real_path = NULL;
	char buf[len]; /* dynamic allocation */
	int ret = 0;
	int dir_fd;
	FILE *f;
	int fd;

	real_path = realpath(base, NULL);
	if (!real_path)
		return -errno;

	dir_fd = open(real_path, O_RDONLY | O_CLOEXEC);
	free(real_path);
	if (dir_fd == -1)
		return -errno;

	fd = openat(dir_fd, filename, O_RDONLY);
	if (fd == -1) {
		close(dir_fd);
		return -errno;
	}

	f = fdopen(fd, "r");
	if (f == NULL) {
		close(fd);
		close(dir_fd);
		return -errno;
	}

	if (fgets(buf, len, f) == NULL)
		ret = -errno;

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

static int topo_parse_cpu_node_info(struct wayca_topo *p_topo, int cpu_index)
{
	char path_buffer[WAYCA_SC_PATH_LEN_MAX];
	static int max_node_index = -1; /* actual node_index starts from 0 */
	struct dirent *dirent;
	long node_index;
	char *endptr;
	DIR *dir;

	snprintf(path_buffer, sizeof(path_buffer), "%s/cpu%d",
			WAYCA_SC_CPU_FNAME, cpu_index);
	/* read cpu%d/node* to learn which numa node this cpu belongs to */
	dir = opendir(path_buffer);
	if (!dir)
		return -errno;

	while ((dirent = readdir(dir)) != NULL) {
		if (strncmp(dirent->d_name, "node", 4))
			continue;
		/* read node_index, the sysfs format is "node[0-9]" */
		errno = 0;
		node_index = strtoul(dirent->d_name + 4, &endptr, 0);
		if (errno)
			continue;
		if (endptr == dirent->d_name + 4)
			continue;
		/* check whether need more node space */
		if (node_index > max_node_index) {
			struct wayca_node **p_temp;

			max_node_index = node_index;
			/*
			 * Unnecessary to check the overflow of the
			 * realloc(), max_node_index cannot be that
			 * large.
			 */
			p_temp = (struct wayca_node **)realloc(
				p_topo->nodes, (max_node_index + 1) *
				sizeof(struct wayca_node *));
			if (!p_temp) {
				closedir(dir);
				return -ENOMEM;
			}
			p_topo->nodes = p_temp;
		}
		/*
		 * check this 'node_index' node exist or not. if not, create one
		 */
		if (!CPU_ISSET_S(node_index, CPU_ALLOC_SIZE(p_topo->n_cpus),
				 p_topo->node_map)) {
			p_topo->nodes[node_index] = (struct wayca_node *)calloc(
				1, sizeof(struct wayca_node));
			if (!p_topo->nodes[node_index]) {
				closedir(dir);
				return -ENOMEM;
			}
			p_topo->nodes[node_index]->node_idx = node_index;
			/* initialize this node's cpu_map */
			p_topo->nodes[node_index]->cpu_map =
				CPU_ALLOC(p_topo->kernel_max_cpus);
			if (!p_topo->nodes[node_index]->cpu_map) {
				closedir(dir);
				return -ENOMEM;
			}
			CPU_ZERO_S(p_topo->setsize,
				   p_topo->nodes[node_index]->cpu_map);
			/* add node_index into the top-level node map */
			CPU_SET_S(node_index, CPU_ALLOC_SIZE(p_topo->n_cpus),
				  p_topo->node_map);
			p_topo->n_nodes++;
		}
		/* add current CPU into this node's cpu map */
		CPU_SET_S(cpu_index, p_topo->setsize,
			  p_topo->nodes[node_index]->cpu_map);
		p_topo->nodes[node_index]->n_cpus++;
		/* link this node back to current CPU */
		p_topo->cpus[cpu_index]->p_numa_node =
			p_topo->nodes[node_index];
		break; /* found one "node" entry, no need to check any more */
	}
	closedir(dir);
	return 0;
}

static int topo_parse_cpu_cluster_info(struct wayca_topo *p_topo,
			const char *path_buffer, int cpu_index)
{
	struct wayca_cluster **p_temp;
	int cluster_id;
	int ret;
	int i;

	/* cluster level may not set */
	if (topo_path_read_s32(path_buffer, "cluster_id", &cluster_id) != 0) {
		p_topo->cpus[cpu_index]->p_cluster = NULL;
		return 0;
	}

	/* check this "cluster_id" exists or not */
	for (i = 0; i < p_topo->n_clusters; i++) {
		if (p_topo->ccls[i]->cluster_id == cluster_id)
			break;
	}
	/* need to create a new wayca_cluster */
	if (i == p_topo->n_clusters) {
		p_topo->n_clusters++;
		/* increase p_topo->ccls array */
		p_temp = (struct wayca_cluster **)realloc(
			p_topo->ccls,
			(p_topo->n_clusters) * sizeof(struct wayca_cluster *));
		if (!p_temp)
			return -ENOMEM;
		p_topo->ccls = p_temp;
		/* allocate a new wayca_cluster struct, and link it to p_topo->ccls */
		p_topo->ccls[i] = (struct wayca_cluster *)calloc(
			1, sizeof(struct wayca_cluster));
		if (!p_topo->ccls[i])
			return -ENOMEM;
		/* initialize this cluster's cpu_map */
		p_topo->ccls[i]->cpu_map = CPU_ALLOC(p_topo->kernel_max_cpus);
		if (!p_topo->ccls[i]->cpu_map)
			return -ENOMEM;
		/* read "cluster_cpus_list" */
		ret = topo_path_read_cpulist(path_buffer, "cluster_cpus_list",
					     p_topo->ccls[i]->cpu_map,
					     p_topo->kernel_max_cpus);
		if (ret) {
			PRINT_ERROR(
				"get ccl %d cluster_cpu_list fail, ret = %d\n",
				i, ret);
			return ret;
		}
		/* assign cluster_id and n_cpus */
		p_topo->ccls[i]->cluster_id = cluster_id;
		p_topo->ccls[i]->n_cpus =
			CPU_COUNT_S(p_topo->setsize, p_topo->ccls[i]->cpu_map);
	}
	/* link this cluster back to current CPU */
	p_topo->cpus[cpu_index]->p_cluster = p_topo->ccls[i];
	return 0;
}

static int topo_parse_cpu_pkg_info(struct wayca_topo *p_topo,
			const char *path_buffer, int cpu_index)
{
	int ppkg_id;
	int ret;
	int i;

	/* read "physical_package_id" */
	ret = topo_path_read_s32(path_buffer, "physical_package_id", &ppkg_id);
	if (ret) {
		PRINT_ERROR("get physical_package_id fail, ret = %d\n", ret);
		return ret;
	}
	/* check this "physical_package_id" exists or not */
	for (i = 0; i < p_topo->n_packages; i++)
		if (p_topo->packages[i]->physical_package_id == ppkg_id)
			break;
	/* need to create a new wayca_package */
	if (i == p_topo->n_packages) {
		struct wayca_package **p_temp;

		p_topo->n_packages++;
		p_temp = (struct wayca_package **)realloc(
			p_topo->packages,
			p_topo->n_packages * sizeof(struct wayca_package *));
		if (!p_temp)
			return -ENOMEM;
		p_topo->packages = p_temp;
		/* allocate a new wayca_ package struct, and link it to p_topo->ccls */
		p_topo->packages[i] = (struct wayca_package *)calloc(
			1, sizeof(struct wayca_package));
		if (!p_topo->packages[i])
			return -ENOMEM;
		/* initialize this package's cpu_map */
		p_topo->packages[i]->cpu_map =
			CPU_ALLOC(p_topo->kernel_max_cpus);
		if (!p_topo->packages[i]->cpu_map)
			return -ENOMEM;
		/* initialize this package's numa_map */
		p_topo->packages[i]->numa_map = CPU_ALLOC(p_topo->n_cpus);
		if (!p_topo->packages[i]->numa_map)
			return -ENOMEM;
		CPU_ZERO_S(CPU_ALLOC_SIZE(p_topo->n_cpus),
			   p_topo->packages[i]->numa_map);
		/* read "package_cpus_list" */
		ret = topo_path_read_cpulist(path_buffer, "package_cpus_list",
					     p_topo->packages[i]->cpu_map,
					     p_topo->kernel_max_cpus);
		if (ret) {
			PRINT_ERROR(
				"get package %d package_cpu_list fail, ret = %d\n",
				i, ret);
			return ret;
		}
		/* assign physical_package_id and n_cpus */
		p_topo->packages[i]->physical_package_id = ppkg_id;
		p_topo->packages[i]->n_cpus = CPU_COUNT_S(
			p_topo->setsize, p_topo->packages[i]->cpu_map);
	}
	/* link this package back to current CPU */
	p_topo->cpus[cpu_index]->p_package = p_topo->packages[i];
	return 0;
}

static int topo_parse_cpu_core_info(struct wayca_topo *p_topo,
			const char *path_buffer, int cpu_index)
{
	int core_id;
	int ret;

	/* read "core_id" */
	if (topo_path_read_s32(path_buffer, "core_id", &core_id) != 0)
		p_topo->cpus[cpu_index]->core_id = -1;
	else
		p_topo->cpus[cpu_index]->core_id = core_id;

	/* read core_cpus_list, (SMT: simultaneous multi-threading) */
	p_topo->cpus[cpu_index]->core_cpus_map =
		CPU_ALLOC(p_topo->kernel_max_cpus);
	if (!p_topo->cpus[cpu_index]->core_cpus_map)
		return -ENOMEM;

	/* read "core_cpus_list" */
	ret = topo_path_read_cpulist(path_buffer, "core_cpus_list",
				     p_topo->cpus[cpu_index]->core_cpus_map,
				     p_topo->kernel_max_cpus);
	if (ret) {
		PRINT_ERROR("get cpu %d core_cpus_list fail, ret = %d\n",
			    cpu_index, ret);
		return ret;
	}
	return 0;
}

static int topo_parse_cache_info(struct wayca_cache *cache, const char *path,
				int max_cpus)
{
	int type_len, real_len;
	int ret;

	/* read cache: id, level, type. default set to -1 on failure */
	if (topo_path_read_s32(path, "id", &cache->id) != 0)
		cache->id = -1;
	if (topo_path_read_s32(path, "level", &cache->level) != 0)
		cache->level = -1;
	type_len = topo_path_read_buffer(path, "type", cache->type,
					 WAYCA_SC_ATTR_STRING_LEN - 1);
	cache->type[WAYCA_SC_ATTR_STRING_LEN - 1] = '\0';
	real_len = strlen(cache->type);

	/* remove trailing newline and nonsense chars on success */
	if (type_len <= 0)
		cache->type[0] = '\0';
	else if (cache->type[real_len - 1] == '\n')
		cache->type[real_len - 1] = '\0';

	/* read cache: allocation_policy */
	type_len = topo_path_read_buffer(path, "allocation_policy",
			      cache->allocation_policy,
			      WAYCA_SC_ATTR_STRING_LEN - 1);
	cache->allocation_policy[WAYCA_SC_ATTR_STRING_LEN - 1] = '\0';
	real_len = strlen(cache->allocation_policy);
	if (type_len <= 0)
		cache->allocation_policy[0] = '\0';
	else if (cache->allocation_policy[real_len - 1] == '\n')
		cache->allocation_policy[real_len - 1] = '\0';

	/* read cache: write_policy */
	type_len = topo_path_read_buffer(path, "write_policy",
					 cache->write_policy,
					 WAYCA_SC_ATTR_STRING_LEN - 1);
	cache->write_policy[WAYCA_SC_ATTR_STRING_LEN - 1] = '\0';
	real_len = strlen(cache->write_policy);
	if (type_len <= 0)
		cache->write_policy[0] = '\0';
	else if (cache->write_policy[real_len - 1] == '\n')
		cache->write_policy[real_len - 1] = '\0';

	/* read cache: ways_of_associativity, etc. */
	topo_path_read_s32(path, "ways_of_associativity",
			   (int *)&cache->ways_of_associativity);
	topo_path_read_s32(path, "physical_line_partition",
			   (int *)&cache->physical_line_partition);
	topo_path_read_s32(path, "number_of_sets",
			   (int *)&cache->number_of_sets);
	topo_path_read_s32(path, "coherency_line_size",
			   (int *)&cache->coherency_line_size);

	/* read cache size */
	type_len = topo_path_read_buffer(path, "size", cache->cache_size,
					 WAYCA_SC_ATTR_STRING_LEN - 1);
	real_len = strlen(cache->cache_size);
	if (type_len <= 0)
		cache->cache_size[0] = '\0';
	else if (cache->cache_size[real_len - 1] == '\n')
		cache->cache_size[real_len - 1] = '\0';

	/* read cache: shared_cpu_list */
	cache->shared_cpu_map = CPU_ALLOC(max_cpus);
	if (!cache->shared_cpu_map)
		return -ENOMEM;
	ret = topo_path_read_cpulist(path, "shared_cpu_list",
				     cache->shared_cpu_map, max_cpus);
	if (ret) {
		PRINT_ERROR("failed to read %s/shared_cpu_list, Error code: %d\n",
			    path, ret);
		return ret;
	}
	return 0;
}

static int topo_parse_cpu_cache_info(struct wayca_topo *p_topo, int cpu_index)
{
	char path_buffer[WAYCA_SC_PATH_LEN_MAX];
	struct wayca_cache *p_caches;
	size_t n_caches = 0;
	int ret;
	int i;

	/* count the number of caches exists */
	do {
		/* move the base to "cpu%d/cache/index%zu" */
		snprintf(path_buffer, sizeof(path_buffer),
			 "%s/cpu%d/cache/index%zu", WAYCA_SC_CPU_FNAME,
			 cpu_index, n_caches);
		/* check access */
		if (access(path_buffer, F_OK) != 0) /* doesn't exist */
			break;
		n_caches++;
	} while (1);
	p_topo->cpus[cpu_index]->n_caches = n_caches;

	if (n_caches == 0) {
		PRINT_DBG("no cache exists for CPU %d\n", cpu_index);
		return 0;
	}

	/* allocate wayca_cache matrix */
	p_topo->cpus[cpu_index]->p_caches = (struct wayca_cache *)calloc(
		n_caches, sizeof(struct wayca_cache));
	if (!p_topo->cpus[cpu_index]->p_caches)
		return -ENOMEM;

	p_caches = p_topo->cpus[cpu_index]->p_caches;

	for (i = 0; i < n_caches; i++) {
		/* move the base to "cpu%d/cache/index%zu" */
		snprintf(path_buffer, sizeof(path_buffer),
			"%s/cpu%d/cache/index%d", WAYCA_SC_CPU_FNAME,
			cpu_index, i);

		ret = topo_parse_cache_info(&p_caches[i], path_buffer,
				p_topo->kernel_max_cpus);
		if (ret) {
			PRINT_ERROR("failed to read cpu cache info, ret = %d\n",
				    ret);
			return ret;
		}
	}
	return 0;
}

/* topo_read_cpu_topology() - read cpu%d topoloy, where %d is cpu_index
 *
 * Return negative on error, 0 on success
 */
static int topo_read_cpu_topology(struct wayca_topo *p_topo, int cpu_index)
{
	char path_buffer[WAYCA_SC_PATH_LEN_MAX];
	int ret;

	/* allocate a new struct wayca_cpu */
	p_topo->cpus[cpu_index] =
		(struct wayca_cpu *)calloc(1, sizeof(struct wayca_cpu));
	if (!p_topo->cpus[cpu_index])
		return -ENOMEM;

	p_topo->cpus[cpu_index]->cpu_id = cpu_index;
	ret = topo_parse_cpu_node_info(p_topo, cpu_index);
	if (ret) {
		PRINT_ERROR("parse CPU%d numa information failed, ret = %d\n",
				cpu_index, ret);
		return ret;
	}
	/* move the base to "cpu%d/topology" */
	snprintf(path_buffer, sizeof(path_buffer), "%s/cpu%d/topology",
			WAYCA_SC_CPU_FNAME, cpu_index);

	ret = topo_parse_cpu_core_info(p_topo, path_buffer, cpu_index);
	if (ret) {
		PRINT_ERROR("parse CPU%d core information failed, ret = %d\n",
				cpu_index, ret);
		return ret;
	}

	ret = topo_parse_cpu_cluster_info(p_topo, path_buffer, cpu_index);
	if (ret) {
		PRINT_ERROR("parse CPU%d ccl information failed, ret = %d\n",
				cpu_index, ret);
		return ret;
	}

	ret = topo_parse_cpu_pkg_info(p_topo, path_buffer, cpu_index);
	if (ret) {
		PRINT_ERROR("parse CPU%d pkg information failed, ret = %d\n",
				cpu_index, ret);
		return ret;
	}

	ret = topo_parse_cpu_cache_info(p_topo, cpu_index);
	if (ret) {
		PRINT_ERROR("parse CPU%d cache information failed, ret = %d\n",
				cpu_index, ret);
		return ret;
	}
	return ret;
}

/* topo_read_node_topology() - read node%d topoloy, where %d is node_index
 *
 * Return negative on error, 0 on success
 */
static int topo_read_node_topology(struct wayca_topo *p_topo, int node_index)
{
	cpu_set_t *node_cpu_map;
	char path_buffer[WAYCA_SC_PATH_LEN_MAX];
	struct wayca_meminfo *meminfo_tmp;
	int *distance_array;
	int ret;

	snprintf(path_buffer, sizeof(path_buffer), "%s/node%d",
			WAYCA_SC_NODE_FNAME, node_index);

	/* read node's cpulist */
	node_cpu_map = CPU_ALLOC(p_topo->kernel_max_cpus);
	if (!node_cpu_map)
		return -ENOMEM;

	ret = topo_path_read_cpulist(path_buffer, "cpulist", node_cpu_map,
				     p_topo->kernel_max_cpus);
	if (ret)
		return ret;
	/* check w/ what's previously composed in cpu_topology reading */
	if (!CPU_EQUAL_S(p_topo->setsize, node_cpu_map,
			 p_topo->nodes[node_index]->cpu_map)) {
		PRINT_ERROR("mismatch detected in node%d cpulist read\n",
			    node_index);
		return -EINVAL;
	}
	CPU_FREE(node_cpu_map);

	/* allocate a distance array*/
	distance_array = (int *)calloc(p_topo->n_nodes, sizeof(int));
	if (!distance_array)
		return -ENOMEM;
	/* read node's distance */
	ret = topo_path_read_multi_s32(path_buffer, "distance", p_topo->n_nodes,
				       distance_array);
	if (ret) {
		PRINT_ERROR("get node distance fail, ret = %d\n", ret);
		free(distance_array);
		return ret;
	}

	p_topo->nodes[node_index]->distance = distance_array;

	/* read meminfo */
	meminfo_tmp =
		(struct wayca_meminfo *)calloc(1, sizeof(struct wayca_meminfo));
	if (!meminfo_tmp)
		return -ENOMEM;
	ret = topo_path_parse_meminfo(meminfo_tmp, path_buffer, "meminfo");
	if (ret) {
		PRINT_ERROR("get node meminfo fail, ret = %d\n", ret);
		free(meminfo_tmp);
		return ret;
	}

	p_topo->nodes[node_index]->p_meminfo = meminfo_tmp;

	return 0;
}

/* topo_construct_core_topology
 *  This function takes in wayca_cpus information and construct a wayca_cores
 *  topology.
 * Return negative on error, 0 on success
 */
static int topo_construct_core_topology(struct wayca_topo *p_topo)
{
	struct wayca_core **pp_core;
	int cur_core_id;
	int i, j;

	/* initialization */
	if (p_topo->cores != NULL || p_topo->n_cores != 0) {
		WAYCA_SC_LOG_ERR(
			"duplicated call, wayca_cores has been established\n");
		return -1;
	}

	/* go through all n_cpus */
	for (i = 0; i < p_topo->n_cpus; i++) {
		/* read this cpu's core_id */
		cur_core_id = p_topo->cpus[i]->core_id;
		/* check whether it is already in wayca_core array */
		for (j = 0; j < p_topo->n_cores; j++) {
			if (p_topo->cores[j]->core_id == cur_core_id)
				break; /* it exists, skip */
		}
		/*
		 * cur_core_id exists in p_topo->cores, just go to check
		 * next one
		 */
		if (j < p_topo->n_cores)
			continue;

		/* allocate a new wayca_core if cur_core_id does not exist */
		pp_core = (struct wayca_core **)realloc(
			p_topo->cores,
			(p_topo->n_cores + 1) * sizeof(struct wayca_core *));
		if (!pp_core)
			return -ENOMEM;
		p_topo->cores = pp_core;

		/* j equals p_topo->n_cores */
		p_topo->cores[j] = (struct wayca_core *)calloc(
			1, sizeof(struct wayca_core));
		if (!p_topo->cores[j])
			return -ENOMEM;

		/* increase p_topo->n_cores */
		p_topo->n_cores++;

		/* copy from cpus[i] to cores[j] */
		p_topo->cores[j]->core_id = cur_core_id;
		p_topo->cores[j]->core_cpus_map =
			p_topo->cpus[i]->core_cpus_map;
		p_topo->cores[j]->n_cpus = CPU_COUNT_S(
			p_topo->setsize, p_topo->cores[j]->core_cpus_map);
		p_topo->cores[j]->p_cluster = p_topo->cpus[i]->p_cluster;
		p_topo->cores[j]->p_numa_node = p_topo->cpus[i]->p_numa_node;
		p_topo->cores[j]->p_package = p_topo->cpus[i]->p_package;
		p_topo->cores[j]->n_caches = p_topo->cpus[i]->n_caches;
		p_topo->cores[j]->p_caches = p_topo->cpus[i]->p_caches;
	}
	return 0;
}

static int topo_recursively_read_io_devices(struct wayca_topo *p_topo,
						const char *rootdir);

static int topo_alloc_cpu(struct wayca_topo *p_topo)
{
	cpu_set_t *cpuset_possible;

	/*
	 * read "cpu/kernel_max" to determine maximum size for future memory
	 * allocations
	 */
	if (topo_path_read_s32(WAYCA_SC_CPU_FNAME, "kernel_max",
			       &p_topo->kernel_max_cpus) == 0)
		p_topo->kernel_max_cpus += 1;
	else
		p_topo->kernel_max_cpus = WAYCA_SC_DEFAULT_KERNEL_MAX;
	p_topo->setsize = CPU_ALLOC_SIZE(p_topo->kernel_max_cpus);

	/* read "cpu/possible" to determine number of CPUs */
	cpuset_possible = CPU_ALLOC(p_topo->kernel_max_cpus);
	if (!cpuset_possible)
		return -ENOMEM;

	if (topo_path_read_cpulist(WAYCA_SC_CPU_FNAME, "possible",
				   cpuset_possible,
				   p_topo->kernel_max_cpus) == 0) {
		/* determine number of CPUs in cpuset_possible */
		p_topo->n_cpus = CPU_COUNT_S(p_topo->setsize, cpuset_possible);
		p_topo->cpu_map = cpuset_possible;
		/* allocate wayca_cpu for each CPU */
		p_topo->cpus = (struct wayca_cpu **)calloc(
			p_topo->n_cpus, sizeof(struct wayca_cpu *));
		if (!p_topo->cpus)
			return -ENOMEM;
	} else {
		PRINT_ERROR("failed to read possible CPUs\n");
		return -EINVAL;
	}
	return 0;
}

static int topo_alloc_node_map(struct wayca_topo *p_topo)
{
	p_topo->node_map = CPU_ALLOC(p_topo->n_cpus);
	if (!p_topo->node_map)
		return -ENOMEM;
	CPU_ZERO_S(CPU_ALLOC_SIZE(p_topo->n_cpus), p_topo->node_map);
	return 0;
}

static int topo_construct_cpu_topology(struct wayca_topo *p_topo)
{
	int ret;
	int i;

	/*
	 * read all cpu%d topology, after the loop, following topology has been
	 * established:
	 *  - p_topo->n_nodes
	 *  - p_topo->node_map
	 *  - p_topo->nodes[]
	 *  - p_topo->n_clusters
	 *  - p_topo->ccls[]
	 *  - p_topo->n_packages
	 *  - p_topo->packages[]
	 */
	for (i = 0; i < p_topo->n_cpus; i++) {
		ret = topo_read_cpu_topology(p_topo, i);
		if (ret) {
			PRINT_ERROR("get cpu %d topology fail, ret = %d\n", i,
				    ret);
			return ret;
		}
	}
	return 0;
}

static int topo_construct_numa_topology(struct wayca_topo *p_topo)
{
	cpu_set_t *bitmask;
	size_t setsize;
	int i, j;
	int ret;

	setsize = CPU_ALLOC_SIZE(p_topo->n_cpus);
	bitmask = CPU_ALLOC(p_topo->n_cpus);
	if (!bitmask) {
		ret = -ENOMEM;
		goto cleanup;
	}
	/* read "node/possible" to determine number of numa nodes */
	ret = topo_path_read_cpulist(WAYCA_SC_NODE_FNAME, "possible",
					bitmask, p_topo->n_cpus);
	if (ret) {
		PRINT_ERROR("failed to read possible NODEs\n");
		goto cleanup;
	}

	/* check the n_nodes and node_map in p_topo */
	if (!CPU_EQUAL_S(setsize, bitmask, p_topo->node_map) ||
		CPU_COUNT_S(setsize, bitmask) != p_topo->n_nodes) {
		PRINT_ERROR(
			 "node/possible mismatch with what cpu topology shows\n");
		goto cleanup;
	}
	/* read all node%d topology, and it will establish the distance info */
	for (i = 0; i < p_topo->n_nodes; i++) {
		ret = topo_read_node_topology(p_topo, i);
		if (ret) {
			PRINT_ERROR("get node %d topology fail, ret = %d\n", i,
				    ret);
			goto cleanup;
		}
		/* build the package numa map */
		CPU_ZERO_S(setsize, bitmask);
		for (j = 0; j < p_topo->n_packages; j++) {
			CPU_AND_S(setsize, bitmask,
				  p_topo->packages[j]->cpu_map,
				  p_topo->nodes[i]->cpu_map);
			if (CPU_EQUAL_S(setsize, bitmask,
					p_topo->nodes[i]->cpu_map))
				CPU_SET_S(i, setsize,
					  p_topo->packages[j]->numa_map);
		}
	}
cleanup:
	CPU_FREE(bitmask);
	return ret;
}

static int topo_get_irq_info(struct wayca_topo *sys_topo);

static void topo_init(void)
{
	char origin_wd[WAYCA_SC_PATH_LEN_MAX];
	struct wayca_topo *p_topo = &topo;
	char *p;
	int ret;

	getcwd(origin_wd, WAYCA_SC_PATH_LEN_MAX);
	memset(p_topo, 0, sizeof(struct wayca_topo));

	ret = topo_alloc_cpu(p_topo);
	if (ret) {
		PRINT_ERROR("failed to alloc cpu, ret = %d\n", ret);
		goto cleanup_on_error;
	}

	ret = topo_alloc_node_map(p_topo);
	if (ret) {
		PRINT_ERROR("failed to alloc numa node map, ret = %d\n", ret);
		goto cleanup_on_error;
	}

	ret = topo_construct_cpu_topology(p_topo);
	if (ret) {
		PRINT_ERROR("failed to construct cpu topology, ret = %d\n", ret);
		goto cleanup_on_error;
	}

	ret = topo_construct_numa_topology(p_topo);
	if (ret) {
		PRINT_ERROR("failed to construct numa topology, ret = %d\n", ret);
		goto cleanup_on_error;
	}
	/* Construct wayca_cores topology from wayca_cpus */
	ret = topo_construct_core_topology(p_topo);
	if (ret) {
		PRINT_ERROR("failed to construct core topology, ret = %d\n", ret);
		goto cleanup_on_error;
	}

	if (topo_recursively_read_io_devices(p_topo, WAYCA_SC_SYSDEV_FNAME) !=
	    0) {
		PRINT_ERROR("failed to construct io device topology, ret = %d\n",
				ret);
		goto cleanup_on_error;
	}

	p = secure_getenv("WAYCA_SC_TOPO_GET_IRQ_INFO");
	if (p && !strcmp(p, "YES")) {
		ret = topo_get_irq_info(p_topo);
		if (ret) {
			PRINT_ERROR("failed to get irq information, ret = %d\n",
					ret);
			goto cleanup_on_error;
		}
	}
	/* the working dir may be changed, restore thie working dir */
	chdir(origin_wd);
	return;
	/* cleanup_on_error */
cleanup_on_error:
	topo_free();
	return;
}

/* print the topology */
void topo_print_wayca_cluster(size_t setsize, struct wayca_cluster *p_cluster)
{
	PRINT_DBG("cluster_id: %08x\n", p_cluster->cluster_id);
	PRINT_DBG("n_cpus: %lu\n", p_cluster->n_cpus);
	PRINT_DBG("\tCpu count in this cluster's cpu_map: %d\n",
		  CPU_COUNT_S(setsize, p_cluster->cpu_map));
}

void topo_print_wayca_node(size_t setsize, struct wayca_node *p_node,
			   size_t distance_size)
{
	int i, j;

	PRINT_DBG("node index: %d\n", p_node->node_idx);
	PRINT_DBG("n_cpus: %lu\n", p_node->n_cpus);
	PRINT_DBG("\tCpu count in this node's cpu_map: %d\n",
		  CPU_COUNT_S(setsize, p_node->cpu_map));
	PRINT_DBG("total memory (in kB): %8lu\n",
		  p_node->p_meminfo->total_avail_kB);
	PRINT_DBG("distance: ");
	for (i = 0; i < distance_size; i++)
		PRINT_DBG("%d\t", p_node->distance[i]);
	PRINT_DBG("\n");
	PRINT_DBG("n_pcidevs: %lu\n", p_node->n_pcidevs);
	for (i = 0; i < p_node->n_pcidevs; i++) {
		PRINT_DBG("\tpcidev%d: numa_node=%d\n", i,
			  p_node->pcidevs[i]->numa_node);
		PRINT_DBG("\t\t linked to SMMU No.: %d\n",
			  p_node->pcidevs[i]->smmu_idx);
		PRINT_DBG("\t\t enable(1) or not(0): %d\n",
			  p_node->pcidevs[i]->enable);
		PRINT_DBG("\t\t class=0x%06x\n", p_node->pcidevs[i]->class);
		PRINT_DBG("\t\t vendor=0x%04x\n", p_node->pcidevs[i]->vendor);
		PRINT_DBG("\t\t device=0x%04x\n", p_node->pcidevs[i]->device);
		PRINT_DBG("\t\t number of local CPUs: %d\n",
			  CPU_COUNT_S(setsize,
				      p_node->pcidevs[i]->local_cpu_map));
		PRINT_DBG("\t\t absolute_path: %s\n",
			  p_node->pcidevs[i]->absolute_path);
		PRINT_DBG("\t\t PCI_SLOT_NAME: %s\n",
			  p_node->pcidevs[i]->slot_name);
		PRINT_DBG("\t\t count of irqs (inc. msi_irqs): %d\n",
			  j = p_node->pcidevs[i]->irqs.n_irqs);
		PRINT_DBG("\t\t\t List of IRQs irq_numbers\n");
		for (j = 0; j < p_node->pcidevs[i]->irqs.n_irqs; j++) {
			PRINT_DBG("\t\t\t\t %u:\n",
				  p_node->pcidevs[i]->irqs.irq_numbers[j]);
		}
	}
	PRINT_DBG("n_smmus: %lu\n", p_node->n_smmus);
	for (i = 0; i < p_node->n_smmus; i++) {
		PRINT_DBG("\tSMMU.%d:\n", p_node->smmus[i]->smmu_idx);
		PRINT_DBG("\t\t numa_node: %d\n", p_node->smmus[i]->numa_node);
		PRINT_DBG("\t\t base address : 0x%016"PRIx64"\n",
			  p_node->smmus[i]->base_addr);
		PRINT_DBG("\t\t type(modalias): %s\n",
			  p_node->smmus[i]->modalias);
	}
	/* TODO: fill up the following */
	PRINT_DBG("pointer of cluster_map: 0x%p EXPECTED (nil)\n",
		  p_node->cluster_map);
}

void topo_print_wayca_cpu(size_t setsize, struct wayca_cpu *p_cpu)
{
	PRINT_DBG("cpu_id: %d\n", p_cpu->cpu_id);
	PRINT_DBG("core_id: %d\n", p_cpu->core_id);
	PRINT_DBG("\tCPU count in this core / SMT factor: %d\n",
		  CPU_COUNT_S(setsize, p_cpu->core_cpus_map));
	PRINT_DBG("Number of caches: %zu\n", p_cpu->n_caches);
	for (int i = 0; i < p_cpu->n_caches; i++) {
		PRINT_DBG("\tCache index %d:\n", i);
		PRINT_DBG("\t\tid: %d\n", p_cpu->p_caches[i].id);
		PRINT_DBG("\t\tlevel: %d\n", p_cpu->p_caches[i].level);
		PRINT_DBG("\t\ttype: %s\n", p_cpu->p_caches[i].type);
		PRINT_DBG("\t\tallocation_policy: %s\n",
			  p_cpu->p_caches[i].allocation_policy);
		PRINT_DBG("\t\twrite_policy: %s\n",
			  p_cpu->p_caches[i].write_policy);
		PRINT_DBG("\t\tcache_size: %s\n",
			  p_cpu->p_caches[i].cache_size);
		PRINT_DBG("\t\tways_of_associativity: %u\n",
			  p_cpu->p_caches[i].ways_of_associativity);
		PRINT_DBG("\t\tphysical_line_partition: %u\n",
			  p_cpu->p_caches[i].physical_line_partition);
		PRINT_DBG("\t\tnumber_of_sets: %u\n",
			  p_cpu->p_caches[i].number_of_sets);
		PRINT_DBG("\t\tcoherency_line_size: %u\n",
			  p_cpu->p_caches[i].coherency_line_size);
		PRINT_DBG("\t\tshared with how many cores: %d\n",
			  CPU_COUNT_S(setsize,
				      p_cpu->p_caches[i].shared_cpu_map));
	}
	if (p_cpu->p_cluster != NULL) {
		PRINT_DBG("belongs to cluster_id: \t%08x\n",
			  p_cpu->p_cluster->cluster_id);
	}
	PRINT_DBG("belongs to node: \t%d\n", p_cpu->p_numa_node->node_idx);
	PRINT_DBG("belongs to package_id: \t%08x\n",
		  p_cpu->p_package->physical_package_id);
}

void topo_print_wayca_core(size_t setsize, struct wayca_core *p_core)
{
	PRINT_DBG("core_id: %d\n", p_core->core_id);
	PRINT_DBG("\tn_cpus: %lu\n", p_core->n_cpus);
	PRINT_DBG("\tCPU count in this core / SMT factor: %d\n",
		  CPU_COUNT_S(setsize, p_core->core_cpus_map));
	PRINT_DBG("Number of caches: %zu\n", p_core->n_caches);
	if (p_core->p_cluster != NULL)
		PRINT_DBG("belongs to cluster_id: \t%08x\n",
			  p_core->p_cluster->cluster_id);
	PRINT_DBG("belongs to node: \t%d\n", p_core->p_numa_node->node_idx);
	WAYCA_SC_LOG_INFO("belongs to package_id: \t%08x\n",
			  p_core->p_package->physical_package_id);
	return;
}

#ifdef WAYCA_SC_DEBUG
void wayca_sc_topo_print(void)
{
	struct wayca_topo *p_topo = &topo;
	int i;

	PRINT_DBG("kernel_max_cpus: %d\n", p_topo->kernel_max_cpus);
	PRINT_DBG("setsize: %lu\n", p_topo->setsize);

	PRINT_DBG("n_cpus: %lu\n", p_topo->n_cpus);
	PRINT_DBG("\tCPU count in cpu_map: %d\n",
		  CPU_COUNT_S(p_topo->setsize, p_topo->cpu_map));
	for (i = 0; i < p_topo->n_cpus; i++) {
		if (p_topo->cpus[i] == NULL)
			continue;
		PRINT_DBG("CPU%d information:\n", i);
		topo_print_wayca_cpu(p_topo->setsize, p_topo->cpus[i]);
	}

	PRINT_DBG("n_cores: %lu\n", p_topo->n_cores);
	for (i = 0; i < p_topo->n_cores; i++) {
		if (p_topo->cores[i] == NULL)
			continue;
		PRINT_DBG("core %d information:\n", i);
		topo_print_wayca_core(p_topo->setsize, p_topo->cores[i]);
	}

	PRINT_DBG("n_clusters: %lu\n", p_topo->n_clusters);
	for (i = 0; i < p_topo->n_clusters; i++) {
		if (p_topo->ccls[i] == NULL)
			continue;
		PRINT_DBG("cluster %d information:\n", i);
		topo_print_wayca_cluster(p_topo->setsize, p_topo->ccls[i]);
	}

	PRINT_DBG("n_nodes: %lu\n", p_topo->n_nodes);
	PRINT_DBG("\tnode count in node_map: %d\n",
		  CPU_COUNT_S(CPU_ALLOC_SIZE(p_topo->n_cpus),
			      p_topo->node_map));
	for (i = 0; i < p_topo->n_nodes; i++) {
		if (p_topo->nodes[i] == NULL)
			continue;
		PRINT_DBG("node%d information:\n", i);
		topo_print_wayca_node(p_topo->setsize, p_topo->nodes[i],
				      p_topo->n_nodes);
	}
	PRINT_DBG("n_packages: %lu\n", p_topo->n_packages);

	return;
}
#endif /* WAYCA_SC_DEBUG */

static void topo_cache_free(struct wayca_cache *caches, size_t n_caches)
{
	int i;

	if (!caches)
		return;

	for (i = 0; i < n_caches; i++)
		CPU_FREE(caches[i].shared_cpu_map);

	free(caches);
}

static void topo_cpu_free(struct wayca_cpu **cpus, size_t n_cpus)
{
	int i;

	if (!cpus)
		return;

	for (i = 0; i < n_cpus; i++) {
		if (!cpus[i])
			continue;

		CPU_FREE(cpus[i]->core_cpus_map);
		topo_cache_free(cpus[i]->p_caches, cpus[i]->n_caches);
		free(cpus[i]);
	}
	free(cpus);
}

static void topo_core_free(struct wayca_core **cores, size_t n_cores)
{
	int i;

	/* NOTE: pointers inside wayca_core are freed by wayca_cpu
	 * So, here we only need to free the top-level wayca_core structure
	 */
	if (!cores)
		return;

	for (i = 0; i < n_cores; i++)
		free(cores[i]);
	free(cores);
}

static void topo_ccl_free(struct wayca_cluster **ccls, size_t n_clusters)
{
	int i;

	if (!ccls)
		return;

	for (i = 0; i < n_clusters; i++) {
		if (!ccls[i])
			continue;

		CPU_FREE(ccls[i]->cpu_map);
		free(ccls[i]);
	}
	free(ccls);
}

static void topo_pcidev_free(struct wayca_pci_device **pcidevs,
		size_t n_pcidevs)
{
	int i;

	if (!pcidevs)
		return;

	for (i = 0; i < n_pcidevs; i++) {
		if (!pcidevs[i])
			continue;

		CPU_FREE(pcidevs[i]->local_cpu_map);
		free(pcidevs[i]->irqs.irq_numbers);
		free(pcidevs[i]);
	}
	free(pcidevs);
}

static void topo_smmu_free(struct wayca_smmu **smmus, size_t n_smmus)
{
	int i;

	if (!smmus)
		return;

	for (i = 0; i < n_smmus; i++)
		free(smmus[i]);
	free(smmus);
}

static void topo_node_free(struct wayca_node **nodes, size_t n_nodes)
{
	int i;

	if (!nodes)
		return;

	for (i = 0; i < n_nodes; i++) {
		if (!nodes[i])
			continue;

		CPU_FREE(nodes[i]->cpu_map);
		CPU_FREE(nodes[i]->cluster_map);
		free(nodes[i]->distance);
		free(nodes[i]->p_meminfo);
		topo_pcidev_free(nodes[i]->pcidevs, nodes[i]->n_pcidevs);
		topo_smmu_free(nodes[i]->smmus, nodes[i]->n_smmus);
		free(nodes[i]);
	}
	free(nodes);
}

static void topo_package_free(struct wayca_package **packages,
		size_t n_packages)
{
	int i;

	if (!packages)
		return;

	for (i = 0; i < n_packages; i++) {
		if (!packages[i])
			continue;

		CPU_FREE(packages[i]->cpu_map);
		CPU_FREE(packages[i]->numa_map);
		free(packages[i]);
	}
	free(packages);
}

static void topo_irq_free(struct wayca_irq **irqs, size_t n_irqs);

/* topo_free - free up memories */
void topo_free(void)
{
	struct wayca_topo *p_topo = &topo;

	CPU_FREE(p_topo->cpu_map);
	topo_cpu_free(p_topo->cpus, p_topo->n_cpus);

	topo_core_free(p_topo->cores, p_topo->n_cores);
	topo_ccl_free(p_topo->ccls, p_topo->n_clusters);

	CPU_FREE(p_topo->node_map);
	topo_node_free(p_topo->nodes, p_topo->n_nodes);
	topo_package_free(p_topo->packages, p_topo->n_packages);
	topo_irq_free(p_topo->irqs, p_topo->n_irqs);

	memset(p_topo, 0, sizeof(struct wayca_topo));
	return;
}

int wayca_sc_cpus_in_core(void)
{
	if (topo.n_cores < 1)
		return -ENODATA; /* not initialized */
	return topo.cores[0]->n_cpus;
}

int wayca_sc_cpus_in_ccl(void)
{
	if (topo.n_clusters < 1)
		return -ENODATA; /* not initialized */
	return topo.ccls[0]->n_cpus;
}

int wayca_sc_cpus_in_node(void)
{
	if (topo.n_nodes < 1)
		return -ENODATA; /* not initialized */
	return topo.nodes[0]->n_cpus;
}

int wayca_sc_cpus_in_package(void)
{
	if (topo.n_packages < 1)
		return -ENODATA; /* not initialized */
	return topo.packages[0]->n_cpus;
}

int wayca_sc_cpus_in_total(void)
{
	if (topo.n_cpus < 1)
		return -ENODATA; /* not initialized */
	return topo.n_cpus;
}

int wayca_sc_cores_in_ccl(void)
{
	if (topo.n_clusters < 1)
		return -ENODATA; /* not initialized */
	if (topo.n_cores < 1)
		return -ENODATA; /* not initialized */
	return topo.ccls[0]->n_cpus / topo.cores[0]->n_cpus;
}

int wayca_sc_cores_in_node(void)
{
	if (topo.n_cores < 1)
		return -ENODATA; /* not initialized */
	return topo.nodes[0]->n_cpus / topo.cores[0]->n_cpus;
}

int wayca_sc_cores_in_package(void)
{
	if (topo.n_cores < 1)
		return -ENODATA; /* not initialized */
	return topo.packages[0]->n_cpus / topo.cores[0]->n_cpus;
}

int wayca_sc_cores_in_total(void)
{
	if (topo.n_cores < 1)
		return -ENODATA; /* not initialized */
	return topo.n_cores;
}

int wayca_sc_ccls_in_package(void)
{
	if (topo.n_clusters < 1)
		return -ENODATA; /* not initialized */
	return topo.packages[0]->n_cpus / topo.ccls[0]->n_cpus;
}

int wayca_sc_ccls_in_node(void)
{
	if (topo.n_clusters < 1)
		return -ENODATA; /* not initialized */
	return topo.nodes[0]->n_cpus / topo.ccls[0]->n_cpus;
}

int wayca_sc_ccls_in_total(void)
{
	if (topo.n_clusters < 1)
		return -ENODATA; /* not initialized */
	return topo.n_clusters;
}

int wayca_sc_nodes_in_package(void)
{
	if (topo.n_packages < 1)
		return -ENODATA; /* not initialized */
	return topo.packages[0]->n_cpus / topo.nodes[0]->n_cpus;
}

int wayca_sc_nodes_in_total(void)
{
	if (topo.n_nodes < 1)
		return -ENODATA; /* not initialized */
	return topo.n_nodes;
}

int wayca_sc_packages_in_total(void)
{
	if (topo.n_packages < 1)
		return -ENODATA; /* not initialized */
	return topo.n_packages;
}

static bool topo_is_valid_cpu(int cpu_id)
{
	return cpu_id >= 0 && cpu_id < wayca_sc_cpus_in_total();
}

static bool topo_is_valid_core(int core_id)
{
	return core_id >= 0 && core_id < wayca_sc_cores_in_total();
}

static bool topo_is_valid_ccl(int ccl_id)
{
	return ccl_id >= 0 && ccl_id < wayca_sc_ccls_in_total();
}

static bool topo_is_valid_node(int node_id)
{
	return node_id >= 0 && node_id < wayca_sc_nodes_in_total();
}

static bool topo_is_valid_package(int package_id)
{
	return package_id >= 0 && package_id < wayca_sc_packages_in_total();
}

int wayca_sc_core_cpu_mask(int core_id, size_t cpusetsize, cpu_set_t *mask)
{
	size_t valid_cpu_setsize;

	if (mask == NULL || !topo_is_valid_core(core_id))
		return -EINVAL;

	valid_cpu_setsize = CPU_ALLOC_SIZE(topo.n_cpus);
	if (cpusetsize < valid_cpu_setsize)
		return -EINVAL;

	CPU_ZERO_S(cpusetsize, mask);
	CPU_OR_S(valid_cpu_setsize, mask, mask,
		 topo.cores[core_id]->core_cpus_map);
	return 0;
}

int wayca_sc_ccl_cpu_mask(int ccl_id, size_t cpusetsize, cpu_set_t *mask)
{
	size_t valid_cpu_setsize;

	if (mask == NULL || !topo_is_valid_ccl(ccl_id))
		return -EINVAL;

	valid_cpu_setsize = CPU_ALLOC_SIZE(topo.n_cpus);
	if (cpusetsize < valid_cpu_setsize)
		return -EINVAL;

	CPU_ZERO_S(cpusetsize, mask);
	CPU_OR_S(valid_cpu_setsize, mask, mask, topo.ccls[ccl_id]->cpu_map);
	return 0;
}

int wayca_sc_node_cpu_mask(int node_id, size_t cpusetsize, cpu_set_t *mask)
{
	size_t valid_cpu_setsize;

	if (mask == NULL || !topo_is_valid_node(node_id))
		return -EINVAL;

	valid_cpu_setsize = CPU_ALLOC_SIZE(topo.n_cpus);
	if (cpusetsize < valid_cpu_setsize)
		return -EINVAL;

	CPU_ZERO_S(cpusetsize, mask);
	CPU_OR_S(valid_cpu_setsize, mask, mask, topo.nodes[node_id]->cpu_map);
	return 0;
}

int wayca_sc_package_cpu_mask(int package_id, size_t cpusetsize,
			      cpu_set_t *mask)
{
	size_t valid_cpu_setsize;

	if (mask == NULL || !topo_is_valid_package(package_id))
		return -EINVAL;

	valid_cpu_setsize = CPU_ALLOC_SIZE(topo.n_cpus);
	if (cpusetsize < valid_cpu_setsize)
		return -EINVAL;

	CPU_ZERO_S(cpusetsize, mask);
	CPU_OR_S(valid_cpu_setsize, mask, mask,
		 topo.packages[package_id]->cpu_map);
	return 0;
}

int wayca_sc_total_cpu_mask(size_t cpusetsize, cpu_set_t *mask)
{
	size_t valid_cpu_setsize;

	if (mask == NULL)
		return -EINVAL;

	valid_cpu_setsize = CPU_ALLOC_SIZE(topo.n_cpus);
	if (cpusetsize < valid_cpu_setsize)
		return -EINVAL;

	CPU_ZERO_S(cpusetsize, mask);
	CPU_OR_S(valid_cpu_setsize, mask, mask, topo.cpu_map);
	return 0;
}

int wayca_sc_package_node_mask(int package_id, size_t setsize, cpu_set_t *mask)
{
	size_t valid_numa_setsize;

	if (mask == NULL || !topo_is_valid_package(package_id))
		return -EINVAL;

	valid_numa_setsize = CPU_ALLOC_SIZE(topo.n_nodes);
	if (setsize < valid_numa_setsize)
		return -EINVAL;

	CPU_ZERO_S(setsize, mask);
	CPU_OR_S(valid_numa_setsize, mask, mask,
		 topo.packages[package_id]->numa_map);
	return 0;
}

int wayca_sc_total_node_mask(size_t setsize, cpu_set_t *mask)
{
	size_t valid_numa_setsize;

	if (mask == NULL)
		return -EINVAL;

	valid_numa_setsize = CPU_ALLOC_SIZE(topo.n_nodes);
	if (setsize < valid_numa_setsize)
		return -EINVAL;

	CPU_ZERO_S(setsize, mask);
	CPU_OR_S(valid_numa_setsize, mask, mask, topo.node_map);
	return 0;
}

int wayca_sc_get_core_id(int cpu_id)
{
	if (!topo_is_valid_cpu(cpu_id))
		return -EINVAL;

	return topo.cpus[cpu_id]->core_id;
}

int wayca_sc_get_ccl_id(int cpu_id)
{
	int physical_id;
	int i;

	// cluster may not exist in some version of kernel
	if (!topo_is_valid_cpu(cpu_id) || wayca_sc_cpus_in_ccl() < 0)
		return -EINVAL;

	physical_id = topo.cpus[cpu_id]->p_cluster->cluster_id;
	for (i = 0; i < topo.n_clusters; i++) {
		if (topo.ccls[i]->cluster_id == physical_id)
			return i;
	}
	return -EINVAL;
}

int wayca_sc_get_node_id(int cpu_id)
{
	if (!topo_is_valid_cpu(cpu_id))
		return -EINVAL;

	return topo.cpus[cpu_id]->p_numa_node->node_idx;
}

int wayca_sc_get_package_id(int cpu_id)
{
	int physical_id;
	int i;

	if (!topo_is_valid_cpu(cpu_id))
		return -EINVAL;

	physical_id = topo.cpus[cpu_id]->p_package->physical_package_id;
	for (i = 0; i < topo.n_packages; i++) {
		if (topo.packages[i]->physical_package_id == physical_id)
			return i;
	}
	return -EINVAL;
}

int wayca_sc_get_node_mem_size(int node_id, unsigned long *size)
{
	if (size == NULL || !topo_is_valid_node(node_id))
		return -EINVAL;

	*size = topo.nodes[node_id]->p_meminfo->total_avail_kB;
	return 0;
}

static int parse_cache_size(const char *size)
{
	int cache_size;
	char *endstr;

	cache_size = strtol(size, &endstr, 10);
	if (cache_size < 0 || *endstr != 'K')
		return -EINVAL;

	return cache_size;
}

int wayca_sc_get_l1i_size(int cpu_id)
{
	static const char *size;
	static const char *type;
	int level;
	int i;

	if (!topo_is_valid_cpu(cpu_id))
		return -EINVAL;

	for (i = 0; i < topo.cpus[cpu_id]->n_caches; i++) {
		level = topo.cpus[cpu_id]->p_caches[i].level;
		type = topo.cpus[cpu_id]->p_caches[i].type;
		if (level == 1 && !strcmp(type, "Instruction")) {
			size = topo.cpus[cpu_id]->p_caches[i].cache_size;
			return parse_cache_size(size);
		}
	}
	return -ENODATA;
}

int wayca_sc_get_l1d_size(int cpu_id)
{
	static const char *size;
	static const char *type;
	int level;
	int i;

	if (!topo_is_valid_cpu(cpu_id))
		return -EINVAL;

	for (i = 0; i < topo.cpus[cpu_id]->n_caches; i++) {
		level = topo.cpus[cpu_id]->p_caches[i].level;
		type = topo.cpus[cpu_id]->p_caches[i].type;
		if (level == 1 && !strcmp(type, "Data")) {
			size = topo.cpus[cpu_id]->p_caches[i].cache_size;
			return parse_cache_size(size);
		}
	}
	return -ENODATA;
}

int wayca_sc_get_l2_size(int cpu_id)
{
	static const char *size;
	static const char *type;
	int level;
	int i;

	if (!topo_is_valid_cpu(cpu_id))
		return -EINVAL;

	for (i = 0; i < topo.cpus[cpu_id]->n_caches; i++) {
		level = topo.cpus[cpu_id]->p_caches[i].level;
		type = topo.cpus[cpu_id]->p_caches[i].type;
		if (level == 2 && !strcmp(type, "Unified")) {
			size = topo.cpus[cpu_id]->p_caches[i].cache_size;
			return parse_cache_size(size);
		}
	}
	return -ENODATA;
}

int wayca_sc_get_l3_size(int cpu_id)
{
	static const char *size;
	static const char *type;
	int level;
	int i;

	if (!topo_is_valid_cpu(cpu_id))
		return -EINVAL;

	for (i = 0; i < topo.cpus[cpu_id]->n_caches; i++) {
		level = topo.cpus[cpu_id]->p_caches[i].level;
		type = topo.cpus[cpu_id]->p_caches[i].type;
		if (level == 3 && !strcmp(type, "Unified")) {
			size = topo.cpus[cpu_id]->p_caches[i].cache_size;
			return parse_cache_size(size);
		}
	}
	return -ENODATA;
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
	{ 15, 18, 18, 19, 19, 19 }, /* 4 threads */
	{ 0, 23, 23, 24, 24, 24 }, /* 8 threads */
	{ 0, 0, 28, 28, 28, 29 }, /* 12 threads */
	{ 0, 0, 0, 31, 32, 31 }, /* 16 threads */
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
	{ 33, 55, 68, 79 }, /* 24 threads */
	{ 0, 66, 92, 112 }, /* 48 threads */
	{ 0, 0, 99, 130 } /* 72 threads */
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
	{ 19, 5, 9, 6, 9, 11, 9 }, /* 4 threads */
	{ 24, 5, 7, 6, 10, 14, 13 }, /* 8 threads */
	{ 29, 5, 7, 6, 10, 15, 13 }, /* 12 threads */
	{ 31, 5, 7, 6, 10, 16, 13 } /* 16 threads */
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
	{ 13, 6, 4, 4, 4, 4 }, /* 4 threads */
	{ 0, 12, 6, 9, 5, 5 }, /* 8 threads */
	{ 0, 0, 16, 15, 12, 10 }, /* 12 threads */
	{ 0, 0, 0, 17, 14, 12 }, /* 16 threads */
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
	{ 13, 8, 6, 6, 6, 6 }, /* 4 threads */
	{ 0, 14, 9, 9, 8, 8 }, /* 8 threads */
	{ 0, 0, 15, 12, 11, 11 }, /* 12 threads */
	{ 0, 0, 0, 16, 14, 13 }, /* 16 threads */
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
	{ 19, 16, 11, 6 }, /* 24 threads */
	{ 0, 19, 17, 14 }, /* 48 threads */
	{ 0, 0, 17, 9 } /* 72 threads */
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
	{ 21, 15, 14, 8 }, /* 24 threads */
	{ 0, 20, 16, 15 }, /* 48 threads */
	{ 0, 0, 18, 12 } /* 72 threads */
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
	46, 49, 66 /* 2 processes in pipe communitcaiton */
};

/* pipe latency across NUMA nodes */
int pipe_latency_NUMA[4] = {
	/* Same NUMA | Neighbor | Remote | Remote *
	 * diff CCLs |  NUMAs   | NUMA0  | NUMA1  */
};

static const struct {
	enum wayca_sc_irq_chip_name value;
	const char *name;
} irq_chip_name_string[] = {
	{WAYCA_SC_TOPO_CHIP_NAME_INVAL, ""},
	{WAYCA_SC_TOPO_CHIP_NAME_MBIGENV2, "mbigen-v2"},
	{WAYCA_SC_TOPO_CHIP_NAME_ITS_MSI, "ITS-MSI"},
	{WAYCA_SC_TOPO_CHIP_NAME_ITS_PMSI, "ITS-pMSI"},
	{WAYCA_SC_TOPO_CHIP_NAME_GICV3, "GICv3"},
};

static enum wayca_sc_irq_chip_name str2_irq_chip_name(char *buf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irq_chip_name_string); i++) {
		if (!strcmp(buf, irq_chip_name_string[i].name))
			return irq_chip_name_string[i].value;
	}
	return WAYCA_SC_TOPO_CHIP_NAME_INVAL;
}

static const struct {
	enum wayca_sc_irq_type value;
	const char *name;
} irq_type_string[] = {
	{WAYCA_SC_TOPO_TYPE_INVAL, ""},
	{WAYCA_SC_TOPO_TYPE_EDGE, "edge"},
	{WAYCA_SC_TOPO_TYPE_LEVEL, "level"},
};

static enum wayca_sc_irq_type str2_irq_type(char *buf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irq_type_string); i++) {
		if (!strcmp(buf, irq_type_string[i].name))
			return irq_type_string[i].value;
	}
	return WAYCA_SC_TOPO_TYPE_INVAL;
}

static void topo_irq_free(struct wayca_irq **irqs, size_t n_irqs)
{
	int i;

	if (!irqs)
		return;

	for (i = 0; i < n_irqs; i++) {
		if (!irqs[i])
			continue;
		free(irqs[i]);
	}
	free(irqs);
}

static int topo_parse_irq_info(struct wayca_irq *irq, char *irq_number)
{
	char buf[WAYCA_SC_ATTR_STRING_LEN] = {0};
	char path_buffer[WAYCA_SC_PATH_LEN_MAX];
	char *endptr;
	int ret;

	snprintf(path_buffer, sizeof(path_buffer), "/sys/kernel/irq/%s",
			irq_number);
	/*
	 * actions is the irq name, if the action is empty, it is not an active
	 * irq
	 */
	ret = topo_path_read_buffer(path_buffer, "actions", irq->name,
				    WAYCA_SC_ATTR_STRING_LEN - 1);
	if (ret <= 0)
		irq->name[0] = '\0';
	else if (irq->name[strlen(irq->name) - 1] == '\n')
		irq->name[strlen(irq->name) - 1] = '\0';

	ret = topo_path_read_buffer(path_buffer, "chip_name", buf,
				    WAYCA_SC_ATTR_STRING_LEN - 1);
	if (ret <= 0) {
		irq->chip_name = WAYCA_SC_TOPO_CHIP_NAME_INVAL;
	} else {
		if (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = '\0';
		irq->chip_name = str2_irq_chip_name(buf);
	}

	ret = topo_path_read_buffer(path_buffer, "type", buf,
				    WAYCA_SC_ATTR_STRING_LEN - 1);
	if (ret <= 0) {
		irq->type = WAYCA_SC_TOPO_TYPE_INVAL;
	} else {
		if (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = '\0';
		irq->type = str2_irq_type(buf);
	}

	errno = 0;
	irq->irq_number = strtoul(irq_number, &endptr, 10);
	if (endptr == irq_number || errno != 0) {
		if (errno)
			return -errno;
		return -EINVAL;
	}

	return 0;
}

/* topo_get_irq_info- get irq information in system.
 * note: only active irq will be add to topo.
 * Input:
 *   - irq: the irq number to query
 * Return: negative on error, 0 on success.
 */
static int topo_get_irq_info(struct wayca_topo *sys_topo)
{
	struct wayca_irq **irqs;
	struct dirent *entry;
	int nb_irq = 0;
	DIR *proc_dp;
	int ret = 0;
	int i;

	if (sys_topo->irqs)
		return 0;
	/*
	 * we only consider activated irqs, and all activated irqs have been
	 * put in /proc/irq/.
	 */
	proc_dp = opendir("/proc/irq");
	if (!proc_dp) {
		PRINT_ERROR("failed to open directory /proc/irq\n");
		return -errno;
	}
	while ((entry = readdir(proc_dp)) != NULL) {
		if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") &&
				strcmp(entry->d_name, ".."))
			nb_irq++;
	}
	irqs = (struct wayca_irq **)calloc(nb_irq, sizeof(struct wayca_irq *));
	if (!irqs)
		return -ENOMEM;
	rewinddir(proc_dp);

	i = 0;
	while ((entry = readdir(proc_dp)) != NULL && i < nb_irq) {
		if (entry->d_type != DT_DIR || !strcmp(entry->d_name, ".") ||
				!strcmp(entry->d_name, ".."))
			continue;

		irqs[i] = (struct wayca_irq *)calloc(1, sizeof(**irqs));
		if (!irqs[i]) {
			ret = -ENOMEM;
			goto cleanup_on_error;
		}

		ret = topo_parse_irq_info(irqs[i], entry->d_name);
		if (ret)
			goto cleanup_on_error;
		i++;
	}
	sys_topo->irqs = irqs;
	sys_topo->n_irqs = nb_irq;
	closedir(proc_dp);
	return ret;

cleanup_on_error:
	topo_irq_free(irqs, nb_irq);
	closedir(proc_dp);
	return ret;
}

static int topo_parse_msi_irq(struct wayca_device_irqs *wirqs,
				const char *device_sysfs_dir)
{
	char path_buffer[WAYCA_SC_PATH_LEN_MAX];
	int msi_irqs_count = 0;
	uint32_t *irq_numbers;
	struct dirent *entry;
	int i = 0;
	DIR *dp;

	snprintf(path_buffer, sizeof(path_buffer),
			"%s/msi_irqs", device_sysfs_dir);

	dp = opendir(path_buffer);
	if (dp == NULL)
		return -errno;
	while ((entry = readdir(dp)) != NULL) {
		/* If the entry is a regular file */
		if (entry->d_type == DT_REG)
			msi_irqs_count++;
	}
	wirqs->n_irqs = msi_irqs_count;
	PRINT_DBG("found %d interrupts in msi_irqs\n", msi_irqs_count);

	/* allocate irq structs */
	irq_numbers = (uint32_t *)calloc(msi_irqs_count, sizeof(int));
	if (!irq_numbers) {
		closedir(dp);
		return -ENOMEM;
	}
	wirqs->irq_numbers = irq_numbers;

	/* fill in irq_number */
	rewinddir(dp);

	PRINT_DBG("\tinterrupt numbers: ");
	while ((msi_irqs_count != 0) && (entry = readdir(dp)) != NULL) {
		char *endstr;
		/* If the entry is a regular file */
		if (entry->d_type == DT_REG) {
			errno = 0;
			irq_numbers[i] = strtol(entry->d_name, &endstr, 10);
			if (endstr == entry->d_name || errno)
				return errno ? -errno : -EINVAL;
			i++;
			PRINT_DBG("%u\t", irq_numbers[i - 1]);
			msi_irqs_count--;
		}
	}
	PRINT_DBG("\n");
	closedir(dp);
	return 0;
}

static int topo_parse_irq(struct wayca_device_irqs *wirqs,
				const char *device_sysfs_dir)
{
	uint32_t irq_number = 0;
	uint32_t *irq_numbers;
	int irq;
	int j;

	topo_path_read_s32(device_sysfs_dir, "irq", &irq);
	PRINT_DBG("irq file exists, irq number is: %d\n", irq);
	if (irq < 0)
		irq_number = 0; /* on failure, default to set 0 */
	else
		irq_number = (uint32_t)irq;
	/* check it exists in wirqs or not */
	for (j = 0; j < wirqs->n_irqs; j++)
		if (wirqs->irq_numbers[j] == irq_number)
			break;

	/* doesn't exist, create a new one */
	if (j == wirqs->n_irqs) {
		irq_numbers = (uint32_t *)realloc(wirqs->irq_numbers,
				(wirqs->n_irqs + 1) * sizeof(int));
		if (!irq_numbers)
			return -ENOMEM;
		wirqs->n_irqs++;
		wirqs->irq_numbers = irq_numbers;
	}
	return 0;
}

/* parse I/O device irqs information
 * Input: device_sysfs_dir, this device's absolute pathname in /sys/devices
 * Return negative on error, 0 on success
 */
static int topo_parse_device_irqs(struct wayca_device_irqs *wirqs,
					const char *device_sysfs_dir)
{
	int msi_irqs_exist = 0;
	int irq_file_exist = 0;
	struct dirent *entry;
	struct stat statbuf;
	int ret = 0;
	DIR *dp;

	wirqs->n_irqs = 0;

	/* find "msi_irqs" and/or "irq" */
	dp = opendir(device_sysfs_dir);
	if (!dp)
		return -errno;
	while (((msi_irqs_exist == 0) || (irq_file_exist == 0)) &&
	       (entry = readdir(dp)) != NULL) {
		ret = lstat(entry->d_name, &statbuf);
		if (ret < 0) {
			PRINT_ERROR("fail to get directory %s stat, ret = %d\n",
					entry->d_name, -errno);
			continue;
		}

		if ((msi_irqs_exist == 0) && S_ISDIR(statbuf.st_mode)) {
			if (strcmp("msi_irqs", entry->d_name) == 0) {
				msi_irqs_exist = 1;
				PRINT_DBG("found msi_irqs directory under %s\n",
					  device_sysfs_dir);
				continue;
			}
		} else if ((irq_file_exist == 0) &&
			   (strcmp("irq", entry->d_name) == 0)) {
			irq_file_exist = 1;
			PRINT_DBG("found irq file under %s\n",
				  device_sysfs_dir);
			continue;
		}
	}
	closedir(dp);

	if (msi_irqs_exist) {
		ret = topo_parse_msi_irq(wirqs, device_sysfs_dir);
		if (ret) {
			PRINT_ERROR("failed to parse msi irq\n");
			return ret;
		}
	}

	if (irq_file_exist) {
		ret = topo_parse_irq(wirqs, device_sysfs_dir);
		if (ret) {
			PRINT_ERROR("failed to parse irq\n");
			return ret;
		}
	}
	return ret;
}

static int topo_parse_pci_smmu(struct wayca_pci_device *p_pcidev,
				const char *dir)
{
	char path_buffer[WAYCA_SC_PATH_LEN_MAX] = {0};
	char buf_link[WAYCA_SC_PATH_LEN_MAX] = {0};
	char *p_index;

	p_pcidev->smmu_idx = -1; /* initialize */
	snprintf(path_buffer, sizeof(path_buffer), "%s/iommu", dir);
	/* read smmu link */
	if (readlink(path_buffer, buf_link, WAYCA_SC_PATH_LEN_MAX - 1) == -1) {
		if (errno == ENOENT)
			PRINT_DBG(" No IOMMU\n");
		else {
			PRINT_ERROR(
				"failed to read iommu. Error code: %d\n",
				-errno);
			return -errno;
		}
	} else {
		PRINT_DBG("iommu link: %s\n", buf_link);
		/* extract smmu index from buf_link.
		 * the directory is like:
		 * platform/arm-smmu-v3.3.auto/iommu/smmu3.0x0000000140000000
		 * TODO: here is hard-coded name, can not support for different
		 * version of smmu.
		 */
		p_index = strstr(buf_link, "arm-smmu-v3");

		if (p_index == NULL)
			PRINT_ERROR("failed to parse iommu link: %s\n",
					buf_link);
		else {
			p_pcidev->smmu_idx = strtol(
				p_index + strlen("arm-smmu-v3") + 1, NULL, 0);
			PRINT_DBG("smmu index: %d\n", p_pcidev->smmu_idx);
		}
	}
	return 0;
}

static int topo_parse_pci_info(struct wayca_topo *p_topo,
			struct wayca_pci_device *pcidev, const char *dir)
{
	char buf[WAYCA_SC_ATTR_STRING_LEN] = {0};
	int ret;

	/*
	 * read PCI information into: pcidev.
	 * Sysfs data output format is defined in linux kernel code:
	 * [linux.kernel]/drivers/pci/pci-sysfs.c
	 * The format is class:vendor:device
	 */
	ret = topo_path_read_buffer(dir, "class", buf,
				    WAYCA_SC_ATTR_STRING_LEN - 1);
	if (ret <= 0)
		pcidev->class = 0;
	else {
		if (sscanf(buf, "%x", &pcidev->class) != 1)
			pcidev->class = 0;
		PRINT_DBG("class: 0x%06x\n", pcidev->class);
	}
	ret = topo_path_read_buffer(dir, "vendor", buf,
				    WAYCA_SC_ATTR_STRING_LEN - 1);
	if (ret <= 0)
		pcidev->vendor = 0;
	else {
		if (sscanf(buf, "%hx", &pcidev->vendor) != 1)
			pcidev->vendor = 0;
		PRINT_DBG("vendor: 0x%04x\n", pcidev->vendor);
	}
	ret = topo_path_read_buffer(dir, "device", buf,
				    WAYCA_SC_ATTR_STRING_LEN - 1);
	if (ret <= 0)
		pcidev->device = 0;
	else {
		if (sscanf(buf, "%hx", &pcidev->device) != 1)
			pcidev->device = 0;
		PRINT_DBG("device: 0x%04x\n", pcidev->device);
	}
	/* read local_cpulist */
	pcidev->local_cpu_map = CPU_ALLOC(p_topo->kernel_max_cpus);
	if (!pcidev->local_cpu_map)
		return -ENOMEM;
	ret = topo_path_read_cpulist(dir, "local_cpulist",
				     pcidev->local_cpu_map,
				     p_topo->kernel_max_cpus);
	if (ret != 0) {
		PRINT_ERROR("failed to get local_cpulist, ret = %d\n", ret);
		return ret;
	}
	/* read irqs */
	ret = topo_parse_device_irqs(&pcidev->irqs, dir);
	if (ret < 0) {
		PRINT_ERROR("failed to parse irq %s, ret = %d\n", dir, ret);
		return ret;
	}
	/* read enable */
	ret = topo_path_read_s32(dir, "enable", &pcidev->enable);
	if (ret < 0) {
		PRINT_ERROR("failed to read %s/enable, ret = %d\n", dir, ret);
		return ret;
	}
	return 0;
}

static int topo_parse_pci_numa_node(struct wayca_topo *p_topo,
			struct wayca_pci_device *p_pcidev, const char *dir,
			int *numa_id)
{
	int node_nb = -1;
	int i;

	/* read 'numa_node' */
	topo_path_read_s32(dir, "numa_node", &node_nb);
	PRINT_DBG("numa_node: %d\n", node_nb);
	if (node_nb < 0)
		node_nb = 0; /* on failure, default to node #0 */
	p_pcidev->numa_node = node_nb;

	/* get the 'wayca_node *' whoes node_idx == this 'node_nb' */
	for (i = 0; i < p_topo->n_nodes; i++) {
		if (p_topo->nodes[i]->node_idx == node_nb)
			break;
	}
	if (i == p_topo->n_nodes) {
		PRINT_ERROR(
			"failed to match this PCI device to any numa node: %s\n",
			dir);
		free(p_pcidev);
		return -EINVAL;
	}
	*numa_id = i;
	return 0;
}

static int topo_parse_pci_device(struct wayca_topo *p_topo, const char *dir)
{
	struct wayca_pci_device *p_pcidev;
	struct wayca_pci_device **p_temp;
	char *p_index;
	int ret;
	int i;

	PRINT_DBG("PCI full path: %s\n", dir);
	/* allocate struct wayca_pci_device */
	p_pcidev = (struct wayca_pci_device *)calloc(
			1, sizeof(struct wayca_pci_device));
	if (!p_pcidev)
		return -ENOMEM;
	/* store dir full path */
	strncpy(p_pcidev->absolute_path, dir, WAYCA_SC_PATH_LEN_MAX);
	PRINT_DBG("absolute path: %s\n", p_pcidev->absolute_path);

	/*
	 * cut out PCI_SLOT_NAME from the absolute path, which is last part
	 * of the path
	 */
	p_index = rindex(dir, '/');
	if (p_index) {
		strncpy(p_pcidev->slot_name, p_index + 1,
				WAYCA_SC_PCI_SLOT_NAME_LEN_MAX - 1);
		PRINT_DBG("slot_name : %s\n", p_pcidev->slot_name);
	}

	ret = topo_parse_pci_numa_node(p_topo, p_pcidev, dir, &i);
	if (ret < 0) {
		PRINT_ERROR("failed to get pci device node id, ret = %d\n", ret);
		return ret;
	}
	/* append p_pcidev to wayca node[]->pcidevs */
	p_temp = (struct wayca_pci_device **)realloc(p_topo->nodes[i]->pcidevs,
		(p_topo->nodes[i]->n_pcidevs + 1) *
		sizeof(struct wayca_pci_device *));
	if (!p_temp) {
		free(p_pcidev);
		return -ENOMEM;
	}
	p_topo->nodes[i]->pcidevs = p_temp;
	p_topo->nodes[i]->pcidevs[p_topo->nodes[i]->n_pcidevs] = p_pcidev;
	p_topo->nodes[i]->n_pcidevs++;
	PRINT_DBG("n_pcidevs = %zu\n", p_topo->nodes[i]->n_pcidevs);

	ret = topo_parse_pci_info(p_topo, p_pcidev, dir);
	if (ret) {
		PRINT_ERROR("read pci information fail, ret = %d\n", ret);
		return ret;
	}

	ret = topo_parse_pci_smmu(p_pcidev, dir);
	if (ret) {
		PRINT_ERROR("read pci smmu fail, ret = %d\n", ret);
		return ret;
	}
	return ret;
}

static int topo_parse_smmu_info(struct wayca_smmu *p_smmu, const char *dir)
{
	char path_buffer[WAYCA_SC_PATH_LEN_MAX] = {0};
	struct dirent *entry;
	char *p_index;
	int ret;
	DIR *dp;

	p_index = strstr(dir, "arm-smmu-v3");
	if (p_index == NULL) {
		PRINT_ERROR("failed to parse smmu name: %s\n", dir);
		return -EIO;
	}

	strncpy(p_smmu->name, p_index, sizeof(p_smmu->name) - 1);
	PRINT_DBG("smmu name: %s\n", p_smmu->name);
	/* read type (modalias) */
	ret = topo_path_read_buffer(dir, "modalias", p_smmu->modalias,
				    WAYCA_SC_ATTR_STRING_LEN - 1);
	p_smmu->modalias[WAYCA_SC_ATTR_STRING_LEN - 1] = '\0';
	if (ret <= 0)
		p_smmu->modalias[0] = '\0';
	else if (p_smmu->modalias[strlen(p_smmu->modalias) - 1] == '\n')
		p_smmu->modalias[strlen(p_smmu->modalias) - 1] = '\0';
	PRINT_DBG("modalias = %s\n", p_smmu->modalias);
	/*
	 * identify smmu_idx from the 'dir' string
	 * e.g. SMMU full path: /sys/devices/platform/arm-smmu-v3.4.auto
	 * TODO: fix hard-coded name to support different versions of smmu
	 */
	p_index = strstr(dir, "arm-smmu-v3");
	if (p_index == NULL)
		PRINT_ERROR("failed to parse smmu_idx : %s\n", dir);
	else {
		p_smmu->smmu_idx = strtol(
			p_index + strlen("arm-smmu-v3") + 1, NULL, 0);
		PRINT_DBG("smmu index: %d\n", p_smmu->smmu_idx);
	}
	/* read base address */
	snprintf(path_buffer, sizeof(path_buffer), "%s/iommu", dir);
	dp = opendir(path_buffer);
	if (!dp)
		return -errno;

	while ((entry = readdir(dp)) != NULL) {
		/*
		 * Found directory, e.g. iommu/smmu3.0x0000000140000000 to get
		 * smmu base address.
		 */
		p_index = strstr(entry->d_name, "smmu3");
		if (p_index) {
			p_smmu->base_addr = strtoull(
			p_index + strlen("smmu3") + 1, NULL, 0);
			PRINT_DBG("base address : 0x%016"PRIx64"\n",
					p_smmu->base_addr);
			break;
		}
	}

	closedir(dp);
	return 0;
}

static int topo_parse_smmu(struct wayca_topo *p_topo, const char *dir)
{
	struct wayca_smmu **p_temp;
	struct wayca_smmu *p_smmu;
	int node_nb = -1;
	int ret;
	int i;

	PRINT_DBG("SMMU full path: %s\n", dir);
	p_smmu = (struct wayca_smmu *)calloc(1, sizeof(struct wayca_smmu));
	if (!p_smmu)
		return -ENOMEM;
	/* read numa_node */
	topo_path_read_s32(dir, "numa_node", &node_nb);
	PRINT_DBG("numa_node: %d\n", node_nb);
	if (node_nb < 0)
		node_nb = 0; /* on failure, default to node #0 */
	p_smmu->numa_node = node_nb;

	/* get the 'wayca_node *' whoes node_idx == this 'node_nb' */
	for (i = 0; i < p_topo->n_nodes; i++) {
		if (p_topo->nodes[i]->node_idx == node_nb)
			break;
	}
	if (i == p_topo->n_nodes) {
		PRINT_ERROR(
			"failed to match this PCI device to any numa node: %s\n",
			dir);
		free(p_smmu);
		return -EINVAL;
	}

	p_temp = (struct wayca_smmu **)realloc(p_topo->nodes[i]->smmus,
			(p_topo->nodes[i]->n_smmus + 1) *
			sizeof(struct wayca_smmu *));
	if (!p_temp) {
		free(p_smmu);
		return -ENOMEM;
	}
	p_topo->nodes[i]->smmus = p_temp;
	p_topo->nodes[i]->smmus[p_topo->nodes[i]->n_smmus] = p_smmu;
	p_topo->nodes[i]->n_smmus++; /* incement number of SMMU devices */
	PRINT_DBG("n_smmus = %zu\n", p_topo->nodes[i]->n_smmus);

	ret = topo_parse_smmu_info(p_smmu, dir);
	if (ret) {
		PRINT_ERROR("failed to parse smmu information, ret = %d\n", ret);
		return ret;
	}
	return ret;
}

/* Return negative on error, 0 on success
 */
static int topo_parse_io_device(struct wayca_topo *p_topo, const char *dir)
{
	int ret;

	if (!dir)
		return -EINVAL;

	if (strstr(dir, "pci")) {
		ret = topo_parse_pci_device(p_topo, dir);
		if (ret) {
			PRINT_ERROR("parse pci device fail, ret = %d\n", ret);
			return ret;
		}
	} else if (strstr(dir, "smmu")) {
		ret = topo_parse_smmu(p_topo, dir);
		if (ret) {
			PRINT_ERROR("parse smmu fail, ret = %d\n", ret);
			return ret;
		}
	} else {
		/* TODO: support for other device */
		PRINT_DBG("other IO device at full path: %s\n", dir);
	}

	return 0;
}

/* Return negative on error, 0 on success
 */
static int topo_recursively_read_io_devices(struct wayca_topo *p_topo,
						const char *rootdir)
{
	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;
	char cwd[WAYCA_SC_PATH_LEN_MAX];
	int ret = 0;

	dp = opendir(rootdir);
	if (!dp)
		return -errno;

	chdir(rootdir);
	while ((entry = readdir(dp)) != NULL) {
		ret = lstat(entry->d_name, &statbuf);
		if (ret < 0) {
			ret = -errno;
			goto out;
		}

		if (S_ISDIR(statbuf.st_mode)) {
			/* Found a directory, but ignore . and .. */
			if (strcmp(".", entry->d_name) == 0 ||
			    strcmp("..", entry->d_name) == 0)
				continue;
			topo_recursively_read_io_devices(p_topo, entry->d_name);
		} else {
			/*
			 * TODO: We rely on 'numa_node' to represent a
			 * legitimate i/o device. However 'numa_node' exists
			 * only when NUMA is enabled in kernel. So, we Need to
			 * consider a better idea of identifying i/o device.
			 */
			if (strcmp("numa_node", entry->d_name) == 0)
				topo_parse_io_device(p_topo,
					getcwd(cwd, WAYCA_SC_PATH_LEN_MAX));
		}
	}

out:
	chdir("..");
	closedir(dp);
	return ret;
}

int wayca_sc_get_irq_list(size_t *num, uint32_t *irq)
{
	int ret;
	int i;

	if (!num)
		return -EINVAL;

	if (!topo.irqs) {
		ret = topo_get_irq_info(&topo);
		if (ret)
			return ret;
	}

	*num = topo.n_irqs;
	if (!irq)
		return 0;

	for (i = 0; i < topo.n_irqs; i++)
		irq[i] = topo.irqs[i]->irq_number;
	return 0;
}

int wayca_sc_get_irq_info(uint32_t irq_num,
		struct wayca_sc_irq_info *irq_info)
{
	int ret;
	int i;

	if (!irq_info)
		return -EINVAL;
	memset(irq_info, 0, sizeof(*irq_info));

	if (!topo.irqs) {
		ret = topo_get_irq_info(&topo);
		if (ret)
			return ret;
	}

	for (i = 0; i < topo.n_irqs; i++) {
		if (topo.irqs[i]->irq_number == irq_num)
			break;
	}
	if (i == topo.n_irqs)
		return -ENOENT;

	irq_info->irq_num = topo.irqs[i]->irq_number;
	irq_info->chip_name = topo.irqs[i]->chip_name;
	irq_info->type = topo.irqs[i]->type;
	irq_info->name = topo.irqs[i]->name;
	return 0;
}

int wayca_sc_get_device_list(int numa_node, size_t *num, const char **name)
{
	int start_node, end_node;
	int i, j, k;

	if (numa_node >= wayca_sc_nodes_in_total() || !num)
		return -EINVAL;

	*num = 0;
	if (numa_node < 0) {
		start_node = 0;
		end_node = topo.n_nodes - 1;
	} else {
		start_node = numa_node;
		end_node = numa_node;
	}

	for (i = start_node; i <= end_node; i++)
		*num += topo.nodes[i]->n_pcidevs + topo.nodes[i]->n_smmus;

	if (!name)
		return 0;

	for (i = 0, j = start_node; j <= end_node; j++) {
		for (k = 0; k < topo.nodes[j]->n_smmus; k++, i++)
			name[i] = topo.nodes[j]->smmus[k]->name;

		for (k = 0; k < topo.nodes[j]->n_pcidevs; k++, i++)
			name[i] = topo.nodes[j]->pcidevs[k]->slot_name;
	}

	return 0;
}

static void topo_copy_smmu_info(struct wayca_sc_device_info *dev_info,
			 struct wayca_smmu *smmu)
{
	dev_info->name = smmu->name;
	dev_info->smmu_idx = smmu->smmu_idx;
	dev_info->numa_node = smmu->numa_node;
	dev_info->base_addr = smmu->base_addr;
	dev_info->modalias = smmu->modalias;
}

static void topo_copy_pcidev_info(struct wayca_sc_device_info *dev_info,
			   struct wayca_pci_device *pcidev)
{
	dev_info->name = pcidev->slot_name;
	dev_info->smmu_idx = pcidev->smmu_idx;
	dev_info->numa_node = pcidev->numa_node;
	dev_info->device = pcidev->device;
	dev_info->vendor = pcidev->vendor;
	dev_info->class = pcidev->class;
	dev_info->nb_irq = pcidev->irqs.n_irqs;
	dev_info->irq_numbers = pcidev->irqs.irq_numbers;
}

int wayca_sc_get_device_info(const char *name, struct wayca_sc_device_info *dev_info)
{
	int i, j, k;

	if (!dev_info || !name)
		return -EINVAL;
	memset(dev_info, 0, sizeof(*dev_info));

	for (i = 0, j = 0; j < topo.n_nodes; j++) {
		for (k = 0; k < topo.nodes[j]->n_smmus; k++, i++) {
			struct wayca_smmu *smmu = topo.nodes[j]->smmus[k];

			if (!strcmp(smmu->name, name)) {
				dev_info->dev_type =
					WAYCA_SC_TOPO_DEV_TYPE_SMMU;
				topo_copy_smmu_info(dev_info, smmu);
				return 0;
			}
		}

		for (k = 0; k < topo.nodes[j]->n_pcidevs; k++, i++) {
			struct wayca_pci_device *pcidev =
						topo.nodes[j]->pcidevs[k];

			if (!strcmp(pcidev->slot_name, name)) {
				dev_info->dev_type =
					WAYCA_SC_TOPO_DEV_TYPE_PCI;
				topo_copy_pcidev_info(dev_info, pcidev);
				return 0;
			}
		}
	}
	return -ENOENT;
}

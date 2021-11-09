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
#include <errno.h>
#include <sched.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <wayca-scheduler.h>

#include "bitmap.h"
/*
 * Bitmap printing & parsing functions: first version by Bill Irwin,
 * second version by Paul Jackson, third by Joe Korty.
 */
#define CHUNKSZ				32

/**
 * bitmap_scnprintf - convert bitmap to an ASCII hex string.
 * @buf: byte buffer into which string is placed
 * @buflen: reserved size of @buf, in bytes
 * @maskp: pointer to bitmap to convert
 * @nmaskbits: size of bitmap, in bits
 *
 * Exactly @nmaskbits bits are displayed.  Hex digits are grouped into
 * comma-separated sets of eight digits per set.
 */
static int bitmap_scnprintf(char *buf, unsigned int buflen,
			    const unsigned long *maskp, int nmaskbits)
{
	int i, word, len = 0;
	const char *sep = "";
	unsigned int chunksz;
	unsigned long val;
	unsigned int bit;
	uint32_t chunkmask;
	int first = 1;

	chunksz = (unsigned int)nmaskbits & (CHUNKSZ - 1);
	if (chunksz == 0)
		chunksz = CHUNKSZ;

	i = ALIGN((unsigned int)nmaskbits, CHUNKSZ) - CHUNKSZ;
	for (; i >= 0; i -= CHUNKSZ) {
		chunkmask = ((1ULL << chunksz) - 1);
		word = i / BITS_PER_LONG;
		bit = i % BITS_PER_LONG;
		val = (maskp[word] >> bit) & chunkmask;
		if (val != 0 || !first || i == 0) {
			len += snprintf(buf + len, buflen - len, "%s%0*lx", sep,
					(chunksz + 3) / 4, val);
			sep = ",";
			first = 0;
		}
		chunksz = CHUNKSZ;
	}
	return len;
}

static int bitmap_str_to_cpumask(const char *start, size_t len,
				 cpu_set_t *cpuset)
{
	int i, pos, num;
	char *c, *tmp;

	c = strchr(start, '\n');
	if (!c)
		return -EINVAL;

	CPU_ZERO(cpuset);

	pos = i = 0;

	while (c != start) {
		tmp = --c;
		if (*tmp == ',' || *tmp == '\n')
			continue;

		num = hex_to_bin(*tmp);
		if (num < 0)
			return -EINVAL;

		*((unsigned int *)cpuset + pos) |= ((unsigned int)num << i);
		i += 4;
		if (i >= CHUNKSZ) {
			i = 0;
			pos++;
		}
	}

	return 0;
}

int wayca_sc_irq_bind_cpu(int irq, int cpu)
{
	char buf[PATH_MAX];
	cpu_set_t mask;
	int ret;
	int fd;

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);

	snprintf(buf, PATH_MAX, "/proc/irq/%i/smp_affinity", irq);
	fd = open(buf, O_WRONLY, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return -errno;

	bitmap_scnprintf(buf, PATH_MAX, (unsigned long *)&mask,
			 wayca_sc_cpus_in_total());
	ret = write(fd, buf, strlen(buf) + 1);

	close(fd);
	if (ret < 0)
		return -errno;

	return 0;
}

int wayca_sc_get_irq_bind_cpu(int irq, size_t cpusetsize, cpu_set_t *cpuset)
{
	size_t valid_cpu_setsize;
	int fd, ret;
	char buf[PATH_MAX];
	cpu_set_t mask;

	if (!cpuset)
		return -EINVAL;

	snprintf(buf, PATH_MAX, "/proc/irq/%i/smp_affinity", irq);
	fd = open(buf, O_RDONLY, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return -errno;

	memset(buf, 0, PATH_MAX);
	ret = read(fd, buf, PATH_MAX - 1);
	close(fd);

	if (ret < 0)
		return -errno;

	ret = bitmap_str_to_cpumask(buf, ret, &mask);
	if (ret)
		return ret;

	valid_cpu_setsize = CPU_ALLOC_SIZE(wayca_sc_cpus_in_total());
	if (cpusetsize < valid_cpu_setsize)
		return -EINVAL;

	CPU_OR_S(cpusetsize, cpuset, cpuset, &mask);
	return 0;
}

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

/*
 * The test program for testing the bitmap util of Wayca scheduler.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "../lib/bitops.h"
#include "../lib/common.h"

enum bitop_funcs {
	FFZB = 0,	/* find_first_zero_bit */
	FFB,		/* find_first_bit */
	FLB,		/* find_last_bit */
	FNB,		/* find_next_bit */
	FNZB,		/* find_next_zero_bit */
	FUNC_MAX,
};

static unsigned long single_word_cases[] = {
	0x0000000000000000,
	0xffffffffffffffff,
	0x000000ffff000000,
	0xffffffff00000000,
	0x00000000ffffffff,
	0xf0f0f0f0f0f0f0f0,
	0x0f0f0f0f0f0f0f0f,
	0x00ffff0000ffff00,
	0xff0000ffff0000ff,
	0xffff00000000ffff,
};

static int single_word_results[][FUNC_MAX] = {
	{ 0, BITS_PER_LONG, BITS_PER_LONG, -1, -1 },
	{ BITS_PER_LONG, 0, BITS_PER_LONG - 1, -1, -1 },
	{ 0, 24, 39, -1, -1 },
	{ 0, 32, BITS_PER_LONG - 1, -1, -1 },
	{ 32, 0, 31, -1, -1 },
	{ 0, 4, BITS_PER_LONG - 1, -1, -1 },
	{ 4, 0, BITS_PER_LONG - 5, -1, -1 },
	{ 0, 8, BITS_PER_LONG - 9, -1, -1 },
	{ 8, 0, BITS_PER_LONG - 1, -1, -1 },
	{ 16, 0, BITS_PER_LONG - 1, -1, -1 },
};

static void cpu_set_case(void)
{
	size_t cpusetsize;
	cpu_set_t *cpuset;
	unsigned long bit;

	cpuset = CPU_ALLOC(999); /* Use an odd count intentionally */
	if (!cpuset)
		return;

	cpusetsize = CPU_ALLOC_SIZE(999);
	CPU_ZERO_S(cpusetsize, cpuset);

	/* Init the bitmap */
	CPU_SET_S(7, cpusetsize, cpuset);
	CPU_SET_S(8, cpusetsize, cpuset);
	CPU_SET_S(9, cpusetsize, cpuset);
	CPU_SET_S(128, cpusetsize, cpuset);
	CPU_SET_S(222, cpusetsize, cpuset);
	CPU_SET_S(223, cpusetsize, cpuset);
	CPU_SET_S(987, cpusetsize, cpuset);

	bit = find_first_zero_bit((unsigned long *)cpuset, 999);
	assert(bit == 0);

	bit = find_first_bit((unsigned long *)cpuset, 999);
	assert(bit == 7);

	bit = find_last_bit((unsigned long *)cpuset, 999);
	assert(bit == 987);

	bit = find_next_bit((unsigned long *)cpuset, 999, 230);
	assert(bit == 987);

	bit = find_next_zero_bit((unsigned long *)cpuset, 999, 93);
	assert(bit == 93);

	bit = find_next_zero_bit((unsigned long *)cpuset, 999, 222);
	assert(bit == 224);

	printf("cpuset tests passed!\n");
}

int main(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(single_word_cases); i++) {
		unsigned long number = single_word_cases[i];
		unsigned long pos = random() % (BITS_PER_LONG / 2);
		unsigned long bit;

		printf("Case is [%016lx]\n", single_word_cases[i]);
		bit = find_first_zero_bit(&number, BITS_PER_LONG);
		printf("First zero bit: %lu\n", bit);
		assert(bit == single_word_results[i][FFZB]);

		bit = find_first_bit(&number, BITS_PER_LONG);
		printf("First bit: %lu\n", bit);
		assert(bit == single_word_results[i][FFB]);

		bit = find_last_bit(&number, BITS_PER_LONG);
		printf("Last bit: %lu\n", bit);
		assert(bit == single_word_results[i][FLB]);

		bit = find_next_bit(&number, BITS_PER_LONG, pos);
		printf("Next bit from %lu: %lu\n", pos, bit);

		bit = find_next_zero_bit(&number, BITS_PER_LONG, pos);
		printf("Next zero bit from %lu: %lu\n", pos, bit);
	}

	cpu_set_case();

	return 0;
}

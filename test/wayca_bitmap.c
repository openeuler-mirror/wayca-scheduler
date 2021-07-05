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

#include <stdlib.h>
#include <stdio.h>

#include "../lib/bitmap.h"
#include "../lib/common.h"

unsigned long cases[] = {
	0xffff0000,
	0x0000ffff,
	0xf0f0f0f0,
	0x0f0f0f0f,
	0x00ffff00,
	0xff0000ff,
};

int main()
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		unsigned long number = cases[i];
		unsigned long pos = random() % (BITS_PER_LONG / 2);

		printf("Case is [%08lx]\n", cases[i]);
		printf("First zero bit: %lu\n",
		       find_first_zero_bit(&number, BITS_PER_LONG));
		printf("First bit: %lu\n",
		       find_first_bit(&number, BITS_PER_LONG));
		printf("Last bit: %lu\n",
		       find_last_bit(&number, BITS_PER_LONG));
		printf("Next bit from %lu: %lu\n", pos,
		       find_next_bit(&number, BITS_PER_LONG, pos));
		printf("Next zero bit from %lu: %lu\n", pos,
		       find_next_zero_bit(&number, BITS_PER_LONG, pos));
	}

	return 0;
}

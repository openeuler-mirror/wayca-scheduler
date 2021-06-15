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

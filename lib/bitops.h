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
 * Bitops utils.
 */ 
#ifndef LIB_BITOPS_H
#define LIB_BITOPS_H

#include <ctype.h>
#include <sched.h>
#include <sys/types.h>

#include "common.h"

#define likely(cond)	__builtin_expect(!!(cond), 1)
#define unlikely(cond)	__builtin_expect(!!(cond), 0)

#define BITS_PER_LONG	__WORDSIZE

#define ALIGN(x, a) (((x) + (a) - 1UL) & ~((a) - 1UL))

#define small_const_nbits(nbits) \
	(__builtin_constant_p(nbits) && (nbits) <= BITS_PER_LONG && (nbits) > 0)

#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

/**
 * hex_to_bin - convert a hex digit to its real value
 * @ch: ascii character represents hex digit
 *
 * hex_to_bin() converts one hex digit to its actual value or -1 in case of bad
 * input.
 */
static inline int hex_to_bin(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	ch = tolower(ch);
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	return -1;
}

/*
 * Make use of builtin functions for __ffs/__ffz/__fls. Return BITS_PER_LONG if
 * no bit found.
 */
static inline unsigned long __ffs(const unsigned long word)
{
	return word ? __builtin_ffsl(word) - 1 : BITS_PER_LONG;
}

#define __ffz(x)  __ffs(~(x))

static inline unsigned long __fls(const unsigned long word)
{
	return word ? BITS_PER_LONG - __builtin_clzl(word) - 1 : BITS_PER_LONG;
}

/*
 * Caller should make sure start is within nbits
 */
static inline 
unsigned long _find_next_bit(const unsigned long *addr, unsigned long nbits,
			     unsigned long start, unsigned long invert)
{
	unsigned long tmp, mask, ffs;

	WAYCA_SC_ASSERT(start < nbits);

	tmp = addr[start / BITS_PER_LONG];
	tmp ^= invert;
	mask = (~0UL << ((start) & (BITS_PER_LONG - 1)));
	tmp &= mask;
	start = round_down(start, BITS_PER_LONG);

	while (!tmp) {
		start += BITS_PER_LONG;
		if (start >= nbits)
			return nbits;

		tmp = addr[start / BITS_PER_LONG];
		tmp ^= invert;
	}

	ffs = start + __ffs(tmp);
	return min(ffs, nbits);
}

static inline
unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
			    unsigned long offset)
{
	if (unlikely(offset >= size))
		return size;

	if (small_const_nbits(size)) {
		unsigned long val;

		val = *addr & GENMASK(size - 1, offset);
		return val ? __ffs(val) : size;
	}

	return _find_next_bit(addr, size, offset, 0UL);
}

static inline
unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size,
				 unsigned long offset)
{
	if (unlikely(offset >= size))
		return size;

	if (small_const_nbits(size)) {
		unsigned long val;

		val = *addr | ~GENMASK(size - 1, offset);
		return val == ~0UL ? size : __ffz(val);
	}

	return _find_next_bit(addr, size, offset, ~0UL);
}

static inline
unsigned long find_first_bit(const unsigned long *addr, unsigned long size)
{
	unsigned long idx, ffs;

	if (small_const_nbits(size)) {
		unsigned long val = *addr & GENMASK(size - 1, 0);

		return val ? __ffs(val) : size;
	}

	for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
		if (addr[idx]) {
			ffs = idx * BITS_PER_LONG + __ffs(addr[idx]);
			return min(ffs, size);
		}
	}

	return size;
}

static inline
unsigned long find_last_bit(const unsigned long *addr, unsigned long size)
{
	unsigned long idx;
	unsigned long val;

	if (small_const_nbits(size)) {
		val = *addr & GENMASK(size - 1, 0);

		return val ? __fls(val) : size;
	}

	val = (~0UL >> (-(size) & (BITS_PER_LONG - 1)));
	idx = (size - 1) / BITS_PER_LONG;

	do {
		val &= addr[idx];
		if (val)
			return idx * BITS_PER_LONG + __fls(val);

		val = ~0UL;
	} while (idx--);

	return size;
}

static inline
unsigned long find_first_zero_bit(const unsigned long *addr, unsigned long size)
{
	unsigned long idx, ffz;

	if (small_const_nbits(size)) {
		unsigned long val = *addr | ~GENMASK(size - 1, 0);

		return val == ~0UL ? size : __ffz(val);
	}

	for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
		if (addr[idx] != ~0UL) {
			ffz = idx * BITS_PER_LONG + __ffz(addr[idx]);
			return min(ffz, size);
		}
	}

	return size;
}

#ifdef _GNU_SOURCE
static inline int cpuset_find_first_unset(cpu_set_t *cpusetp)
{
	int pos;

	pos = find_first_zero_bit((unsigned long *)cpusetp, CPU_SETSIZE);

	return pos == CPU_SETSIZE ? -1 : pos;
}

static inline int cpuset_find_first_set(cpu_set_t *cpusetp)
{
	int pos;

	pos = find_first_bit((unsigned long *)cpusetp, CPU_SETSIZE);

	return pos == CPU_SETSIZE ? -1 : pos;
}

static inline int cpuset_find_last_set(cpu_set_t *cpusetp)
{
	int pos;

	pos = find_last_bit((unsigned long *)cpusetp, CPU_SETSIZE);

	return pos == CPU_SETSIZE ? -1 : pos;
}

static inline int cpuset_find_next_set(cpu_set_t *cpusetp, int begin)
{
	int pos;

	pos = find_next_bit((unsigned long *)cpusetp, CPU_SETSIZE, begin + 1);

	return pos == CPU_SETSIZE ? -1 : pos;
}
#endif /* _GNU_SOURCE */

#endif	/* LIB_BITOPS_H */

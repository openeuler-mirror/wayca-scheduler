#define _GNU_SOURCE
#include <sched.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <wayca-scheduler.h>

/*
 * bitmap code is taken from the Linux kernel&irqbalance and minimally adapted for use
 * in userspace
 */

#define BITS_PER_LONG ((int)sizeof(unsigned long)*8)

#define BITS_TO_LONGS(bits) \
        (((bits)+BITS_PER_LONG-1)/BITS_PER_LONG)
#define ALIGN(x,a) (((x)+(a)-1UL)&~((a)-1UL))

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
	int i, word, bit, len = 0;
	unsigned long val;
	const char *sep = "";
	int chunksz;
	uint32_t chunkmask;
	int first = 1;

	chunksz = nmaskbits & (CHUNKSZ - 1);
	if (chunksz == 0)
		chunksz = CHUNKSZ;

	i = ALIGN(nmaskbits, CHUNKSZ) - CHUNKSZ;
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

int irq_bind_cpu(int irq, int cpu)
{
	char buf[PATH_MAX];
	int fd;
	int ret;
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);

	sprintf(buf, "/proc/irq/%i/smp_affinity", irq);
	fd = open(buf, O_WRONLY, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return -1;

	bitmap_scnprintf(buf, PATH_MAX, (unsigned long *)&mask,
			 cores_in_total());
	ret = write(fd, buf, strlen(buf) + 1);

	close(fd);

	return ret;
}

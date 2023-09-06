/*
 * Copyright (c) 2022 HiSilicon Technologies Co., Ltd.
 *
 * wayca-memory-bench is a simple benchmark tool for measuring memory access
 * latency and bandwidth. The princple of this tool is partly referenced to
 * https://akkadia.org/drepper/cpumemory.pdf and lmbench implementation.
 *
 * For the latency measurement, the memory to access is initialized as a circular
 * list with certain stride of each elements. The tool will walk the list in
 * random or sequential order to acquire time elapsed and calculate the latency
 * of each load operation. The method is also know as the "pointer chasing".
 * The latency of different memory hierarchy can be measured by controlling the
 * memory size. The latency across the NUMA nodes and CPU caches can also be
 * measured by setting the initiator cpu and target cpu.
 *
 * The memory bandwidth measurement refers to the well known stream benchmark
 *
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
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <wayca-scheduler.h>

#include "common.h"

#define WAYCA_MEMORY_BENCH	"wayca-memory-bench"


#ifdef WAYCA_SC_DEBUG
#define debug_info(fmt, ...)	fprintf(stdout, fmt, ##__VA_ARGS__)
#else
#define debug_info(fmt, ...)	do { } while (0)
#endif

/*
 * Use 1000 base rather than 2^10 to follow the tradition of
 * saying about memory bandwidth.
 */
#define KB		(1000)
#define MB		(1000 * KB)
#define GB		(1000 * MB)
#define KiB		(1024)
#define MiB		(1024 * KiB)
#define GiB		(1024 * MiB)

#define NS_PER_SEC	1e9

/*
 * Define the data type used in latency and stream bench here for
 * convenience of changing the benchmark pattern and avoid omitting.
 */
typedef long int LATENCY_TYPE;
typedef double STREAM_TYPE;

#define SAMPLES			7
#define DEFAULT_BUF_SZ		(256 * MiB)
#define LATENCY_CNT_PER_LOOP	16
#define STREAM_BUF_INIT_1	1
#define STREAM_BUF_INIT_2	2

static char *version = WAYCA_SCHEDULER_VERSION;
static bool random_access = false;
static bool lat_bench = false; /* Perform latency measurement */
static bool bw_bench = false; /* Perform bandwidth measurement */
static bool use_thp = false;
static bool verbose = false;
static size_t buf_sz = DEFAULT_BUF_SZ;
static size_t page_size = 4096;
static size_t latbench_count;
static int cacheline_size = 64;
static int initiator_cpu = -1;
static int target_cpu = -1;
static int total_cpus = 1;
static int iteration = 1;
static long int parallel = 0;
static int stride = 64;

#define TOTAL_BUFFER_CNT	3
struct buffer_info {
	void *buf_0;
	void *buf_1;
	void *buf_2;
};

enum sync_signals {
	READY,
	START,
	STOP,
};

struct pipe_info {
	int ready[2]; /* Notify manager thread is ready */
	int start[2]; /* Notify workers to start */
	int stop[2];  /* Notify workers to stop */
};

typedef void (*bench_func_t)(struct buffer_info *info);

struct thread_info {
	struct buffer_info *buf;
	struct pipe_info *pipes;
	pthread_t thread;
	bool alloc_thread;
	long int overhead;
	long int total;
	int cpu;

	/* For the allocation thread to prepare the buffer */
	bench_func_t prepare_func;
	bench_func_t overhead_func;
	bench_func_t bench_func;
};

static void verbose_print(FILE *stream, const char *fmt, ...)
{
	va_list ap;

	if (!verbose)
		return;

	va_start(ap, fmt);
	vfprintf(stream, fmt, ap);
	va_end(ap);
}

static int get_current_cpu(void)
{
	int cpu;

	if (syscall(SYS_getcpu, &cpu, NULL, NULL))
		return -1;

	return cpu;
}

static long int measure_execute_time(bench_func_t func, struct buffer_info *buf)
{
	struct timespec begin, end;

	clock_gettime(CLOCK_MONOTONIC, &begin);
	if (func)
		func(buf);
	clock_gettime(CLOCK_MONOTONIC, &end);

	return (end.tv_sec - begin.tv_sec) * NS_PER_SEC +
		end.tv_nsec - begin.tv_nsec;
}

static int thread_synchronize(struct pipe_info *pipes, enum sync_signals sig,
			      bool recv, int num)
{
	int fd, i, ret = -EINVAL;
	char dummy = '0';

	/* Get fd index according to @recv */
	i = recv ? 0 : 1;

	switch (sig) {
	case READY:
		fd = pipes->ready[i];
		break;
	case START:
		fd = pipes->start[i];
		break;
	case STOP:
		fd = pipes->stop[i];
		break;
	default:
		return ret;
	}

	if (recv) {
		for (i = 0; i < num; i++)
			ret = read(fd, &dummy, sizeof(dummy));
	} else {
		for (i = 0; i < num; i++)
			ret = write(fd, &dummy, sizeof(dummy));
	}

	debug_info("thread %ld %s signal %d for %d threads\n",
		   syscall(SYS_gettid), recv ? "receive" : "send",
		   sig, num);

	return ret;
}

static void close_pipes(struct pipe_info *pipes)
{
	if (pipes->ready[0]) {
		close(pipes->ready[0]);
		close(pipes->ready[1]);
	}
	if (pipes->start[0]) {
		close(pipes->start[0]);
		close(pipes->start[1]);
	}
	if (pipes->stop[0]) {
		close(pipes->stop[0]);
		close(pipes->stop[1]);
	}
}

static int create_pipes(struct pipe_info *pipes)
{
	if (pipe(pipes->ready) || pipe(pipes->start) || pipe(pipes->stop)) {
		close_pipes(pipes);
		return -errno;
	}

	return 0;
}

static void *alloc_buffers(struct buffer_info *buf)
{
	size_t total_size = TOTAL_BUFFER_CNT * buf_sz;
	void *addr;

	if (posix_memalign(&addr, page_size, total_size)) {
		perror("posix_memalign");
		return NULL;
	}

	if (use_thp && madvise(addr, total_size, MADV_HUGEPAGE))
		perror("madvise");

	/* Fill the buffer to avoid COW */
	memset(addr, -1, total_size);

	buf->buf_0 = addr;
	buf->buf_1 = addr + buf_sz;
	buf->buf_2 = addr + buf_sz + buf_sz;

	return addr;
}

static void free_buffers(struct buffer_info *buf)
{
	free(buf->buf_0);
}

/*
 * LCG random number generator.
 * https://en.wikipedia.org/wiki/Linear_congruential_generator
 */
static unsigned long lcg_prng(void)
{
	static unsigned long seed = 0;
	static unsigned long A = 1103515245;
	static unsigned long B = 12345;
	static unsigned long M = (unsigned long)1 << 31;

	if (!random_access)
		return stride;

	if (!seed)
		seed = time(NULL);

	seed = (A * seed + B) % M;

	return seed % (page_size / stride) * stride + MiB;
}

static unsigned long fix_stride(void)
{
	return stride;
}

static void init_circular_list(struct buffer_info *buf)
{
	LATENCY_TYPE *list = buf->buf_0;
	unsigned long resident, len;
	unsigned long pos = 0, next;
	unsigned long (*prng)(void);

	len = buf_sz / sizeof(LATENCY_TYPE);
	resident = buf_sz / stride - 1;

	if (random_access)
		prng = lcg_prng;
	else
		prng = fix_stride;

	while (resident--) {
		next = pos + prng() / sizeof(LATENCY_TYPE);
		next %= len;

		while (list[next] != -1)
			next = next + stride / sizeof(next) % len;

		list[pos] = next;
		pos = next;
	}

	list[pos] = 0;
}

static void init_stream_buffer(struct buffer_info *info)
{
	size_t len = buf_sz / sizeof(STREAM_TYPE), i;
	STREAM_TYPE *buf_1, *buf_2;

	buf_1 = info->buf_1;
	buf_2 = info->buf_2;

	for (i = 0; i < len; i++) {
		buf_1[i] = STREAM_BUF_INIT_1;
		buf_2[i] = STREAM_BUF_INIT_2;
	}
}

static void measure_stream_overhead(struct buffer_info *info)
{
	size_t len = buf_sz / sizeof(STREAM_TYPE);
	size_t i;

	for (i = 0; i < len; i++)
#ifdef	GCOV_BUILD
		;
#else
		asm volatile("":::"memory");
#endif
}

static void do_stream_copy(struct buffer_info *info)
{
	size_t len = buf_sz / sizeof(STREAM_TYPE);
	volatile STREAM_TYPE *buf_0;
	volatile STREAM_TYPE *buf_1;
	size_t i;

	buf_0 = info->buf_0;
	buf_1 = info->buf_1;

	for (i = 0; i < len; i++)
		*(buf_0++) = *(buf_1++);
}

#define SCALAR	0xbc
static void do_stream_scale(struct buffer_info *info)
{
	size_t len = buf_sz / sizeof(STREAM_TYPE);
	volatile STREAM_TYPE *buf_0;
	volatile STREAM_TYPE *buf_1;
	size_t i;

	buf_0 = info->buf_0;
	buf_1 = info->buf_1;

	for (i = 0; i < len; i++)
		*(buf_0++) = (STREAM_TYPE)SCALAR * *(buf_1++);
}

static void do_stream_add(struct buffer_info *info)
{
	size_t len = buf_sz / sizeof(STREAM_TYPE);
	volatile STREAM_TYPE *buf_0;
	volatile STREAM_TYPE *buf_1;
	volatile STREAM_TYPE *buf_2;
	size_t i;

	buf_0 = info->buf_0;
	buf_1 = info->buf_1;
	buf_2 = info->buf_2;

	for (i = 0; i < len; i++)
		*(buf_0++) = *(buf_1++) + *(buf_2++);
}

static void do_stream_triad(struct buffer_info *info)
{
	size_t len = buf_sz / sizeof(STREAM_TYPE);
	volatile STREAM_TYPE *buf_0;
	volatile STREAM_TYPE *buf_1;
	volatile STREAM_TYPE *buf_2;
	size_t i;

	buf_0 = info->buf_0;
	buf_1 = info->buf_1;
	buf_2 = info->buf_2;

	for (i = 0; i < len; i++)
		*(buf_0++) = *(buf_1++) + (STREAM_TYPE)SCALAR * *(buf_2++);
}

static void measure_list_walk_overhead(struct buffer_info *info)
{
	int i;

	for (i = 0; i < latbench_count; i += LATENCY_CNT_PER_LOOP)
#ifdef	GCOV_BUILD
		;
#else
		asm volatile("":::"memory");
#endif
}

static void walk_circular_list(struct buffer_info *info)
{
	LATENCY_TYPE *list = info->buf_0, pos = 0;
	volatile LATENCY_TYPE __attribute__((__unused__)) dummy;
	int i;

	for (i = 0; i < latbench_count; i += LATENCY_CNT_PER_LOOP) {
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
		pos = list[pos];
	}
	dummy = pos;
}

static void *bench_thread(void *data)
{
	struct thread_info *info = data;
	long int overhead;

	if (info->cpu >= 0) {
		cpu_set_t *cpuset;

		cpuset = CPU_ALLOC(total_cpus);
		if (!cpuset)
			return NULL;

		CPU_ZERO_S(CPU_ALLOC_SIZE(total_cpus), cpuset);
		CPU_SET(info->cpu, cpuset);
		sched_setaffinity(0, CPU_ALLOC_SIZE(total_cpus), cpuset);
		CPU_FREE(cpuset);

		debug_info("thread %ld bind to cpu %d\n", syscall(SYS_gettid), info->cpu);
	}

	if (info->alloc_thread) {
		info->buf->buf_0 = alloc_buffers(info->buf);

		/*
		 * On memory allocation failure we cannot do the preparation
		 * so just notify the manager thread to handle it.
		 */
		if (info->buf->buf_0 && info->prepare_func)
			info->prepare_func(info->buf);

		/* Notify READY */
		thread_synchronize(info->pipes, READY, false, 1);

		/* Block the cancel signal until the buffer is freed */
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	} else {
		int i;

		/* Wait to START */
		thread_synchronize(info->pipes, START, true, 1);

		info->overhead = LONG_MAX;
		info->total = 0;

		for (i = 0; i < SAMPLES; i++) {
			overhead = measure_execute_time(info->overhead_func,
							info->buf);
			info->overhead = min(info->overhead, overhead);
		}

		i = 0;
		while (info->total < 1 * NS_PER_SEC) {
			info->total += measure_execute_time(info->bench_func, info->buf);
			i++;
		}
		info->total /= i;

		/* Notify READY */
		thread_synchronize(info->pipes, READY, false, 1);
	}

	/* Wait to STOP */
	thread_synchronize(info->pipes, STOP, true, 1);

	if (info->alloc_thread) {
		free_buffers(info->buf);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	}

	debug_info("thread %ld end\n", syscall(SYS_gettid));
	return NULL;
}

static int create_bench_threads(struct thread_info *threads_info, struct buffer_info *buf,
				struct pipe_info *pipes)
{
	int ret, i, created = 0;

	/*
	 * Bind memory allocation to the node of target cpu. Otherwise the
	 * automatic NUMA balancing will migrate our pages.
	 */
	wayca_sc_mem_bind_node(wayca_sc_get_node_id(target_cpu));

	/* Make thread 0 the memory allocation thread */
	threads_info[0].alloc_thread = true;
	threads_info[0].pipes = pipes;
	threads_info[0].buf = buf;
	threads_info[0].cpu = target_cpu;
	ret = pthread_create(&threads_info[0].thread, NULL, bench_thread, &threads_info[0]);
	if (ret)
		goto fail;

	created++;

	/* Make thread 1 the bench thread */
	threads_info[1].pipes = pipes;
	threads_info[1].buf = buf;
	threads_info[1].cpu = initiator_cpu;
	ret = pthread_create(&threads_info[1].thread, NULL, bench_thread, &threads_info[1]);
	if (ret)
		goto fail;

	created++;

	/* Create the non-special threads */
	while (created < parallel) {
		threads_info[created].pipes = pipes;
		threads_info[created].buf = buf;
		threads_info[created].cpu = -1;
		ret = pthread_create(&threads_info[created].thread, NULL, bench_thread,
				     &threads_info[created]);
		if (ret)
			goto fail;

		created++;
	}

	return created;
fail:
	for (i = 0; i < created; i++) {
		pthread_cancel(threads_info[i].thread);
		pthread_join(threads_info[i].thread, NULL);
	}

	return -ret;
}

static void destroy_bench_threads(struct thread_info *threads_info, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		pthread_cancel(threads_info[i].thread);
		pthread_join(threads_info[i].thread, NULL);
	}
}

static long int do_bench(bench_func_t prepare_func,
			      bench_func_t overhead_func,
			      bench_func_t bench_func)
{
	long int overhead = 0, total = -1;
	struct thread_info *threads_info;
	struct pipe_info pipes = {0};
	struct buffer_info buf = {0};
	int t_cnt, i, ret;

	/*
	 * parallel specified by the user indicates number of threads
	 * doing the memory access, which doesn't include the memory
	 * allocation thread.
	 *
	 * When user doesn't specify the parallel number,
	 * o parallel is 2 as we'll create one benching thread and
	 *   one memory allocating thread
	 * o if initiator or target is specified make current thread
	 *   performing the measurement on the specified cpu
	 * o if none of initiator nor target is specified then doing
	 *   the measurement on the current cpu
	 *
	 * When parallel is specified,
	 * o if initiator is specified bind the first thread to the
	 *   initiator, otherwise make initiator the current cpu
	 * o if target is specified make memory allocation thread bind
	 *   to target cpu, otherwise make target cpu equals to the
	 *   initiator cpu
	 */
	if (initiator_cpu < 0)
		initiator_cpu = get_current_cpu();
	if (target_cpu < 0)
		target_cpu = initiator_cpu;

	if (initiator_cpu >= total_cpus || target_cpu >= total_cpus) {
		fprintf(stderr, "invalid CPU specified\n");
		return -EINVAL;
	}

	ret = create_pipes(&pipes);
	if (ret)
		return ret;

	threads_info = calloc(parallel, sizeof(struct thread_info));
	if (!threads_info) {
		close_pipes(&pipes);
		return -ENOMEM;
	}

	/* Initialized the benchmark functions */
	for (i = 0; i < parallel; i++) {
		threads_info[i].prepare_func = prepare_func;
		threads_info[i].overhead_func = overhead_func;
		threads_info[i].bench_func = bench_func;
	}

	t_cnt = create_bench_threads(threads_info, &buf, &pipes);
	if (t_cnt < 0)
		goto out;

	/* Wait for the memory allocation thread to be READY */
	thread_synchronize(&pipes, READY, true, 1);

	/*
	 * Check whether the memory is successfully allocated,
	 * otherwise we cannot continue the measurement so just
	 * exit.
	 */
	if (!buf.buf_0) {
		total = -ENOMEM;
		goto out;
	}

	/* Notify the bench thread to START */
	thread_synchronize(&pipes, START, false, t_cnt - 1);
	/* Wait for the bench thread to be READY */
	thread_synchronize(&pipes, READY, true, t_cnt - 1);
	/* Notify all the threads to STOP */
	thread_synchronize(&pipes, STOP, false, t_cnt);

	overhead = threads_info[1].overhead;
	total = threads_info[1].total;

	verbose_print(stdout, "overhead %ld nsec, total %ld nsec\n", overhead, total);

	/* The result is invalid if total < overhead */
	if (total < overhead) {
		total = 0;
		overhead = EINVAL;
	}
out:
	destroy_bench_threads(threads_info, t_cnt);
	free(threads_info);
	close_pipes(&pipes);

	return total - overhead;
}

static int measure_memory_latency(void)
{
	long int time_elapsed = 0;
	int i, iter = 0;

	printf("Measuring load latency: ");

	for (i = 0; i < iteration; i++) {
		time_elapsed += do_bench(init_circular_list, measure_list_walk_overhead,
					walk_circular_list);
		if (time_elapsed <= 0) {
			verbose_print(stderr, "error %d\n", (int)time_elapsed);
			continue;
		}

		iter++;
	}

	if (!iter) {
		printf("fail to measure load latency\n");
		return -EINVAL;
	}

	printf("%.2f nsec\n", (double) time_elapsed / (iter * (latbench_count + 1)));

	return 0;
}

static int measure_memory_bandwidth(void)
{
	static const bench_func_t bench_fs[] = {
		do_stream_copy,
		do_stream_scale,
		do_stream_add,
		do_stream_triad,
	};
	static const char *bench_name[] = {
		"Stream-copy",
		"Stream-scale",
		"Stream-add",
		"Stream-triad",
	};
	static const int count[] = {
		2,
		2,
		3,
		3,
	};
	long int time_elapsed = 0;
	int i, j, iter;

	printf("Measuring memory bandwidth (1 MB/sec = 1,000,000 Bytes/sec):\n");

	for (i = 0; i < sizeof(bench_fs) / sizeof(bench_fs[0]); i++) {
		iter = 0;

		for (j = 0; j < iteration; j++) {
			time_elapsed += do_bench(init_stream_buffer,
						measure_stream_overhead,
						bench_fs[i]);
			if (time_elapsed <= 0) {
				verbose_print(stderr, "error %d\n", (int)time_elapsed);
				continue;
			}

			iter++;
		}

		if (!iter) {
			fprintf(stderr, "fail to perform %s\n", bench_name[i]);
			return -EINVAL;
		}

		/* By now parallel include 1 allocation threads, so exclude it */
		printf("%s bandwidth: %.2f MB/sec\n", bench_name[i],
		       (double) buf_sz * count[i] * iter * (parallel - 1) /
		       MB / time_elapsed * NS_PER_SEC);
	}

	return 0;
}

static void init_bench_parameters(void)
{
	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0) {
		printf("Cannot get page size, assuming using 4K page\n");
		page_size = 4 * KiB;
	}

	cacheline_size = wayca_sc_get_l3_size(0);
	if (cacheline_size <= 0) {
		printf("Cannot get L3 cacheline size, assuming using 64B cacheline\n");
		cacheline_size = 64; /* Assume using 64B cacheline */
	}

	total_cpus = wayca_sc_cpus_in_total();
	if (total_cpus <= 0)
		total_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (total_cpus <= 0) {
		printf("Cannot get total cpu number, assuming total cpu is 1\n");
		total_cpus = 1;
	}

	latbench_count = buf_sz / stride * LATENCY_CNT_PER_LOOP;
}

static void show_version(void)
{
	printf("%s version %s\n", WAYCA_MEMORY_BENCH, version);
}

static void usage(void)
{
	show_version();
	printf("Usage: %s [options]\n"
	       "Options:\n"
	       "-l, --length <len>[K|M|G]	the length of the memory to test, default 256MiB\n"
	       "-i, --initiator <cpu>		the cpu going to access the memory, default to be the\n"
	       "				tool's current running cpu\n"
	       "-t, --target <cpu>		the cpu to allocate the memory, default to be the\n"
	       "				initiator cpu\n"
	       "-s, --stride <stride>		stride length in Bytes of the list elements, default\n"
	       "				to be the L3 cacheline size. Apply to the latency\n"
	       "				measurement only\n"
	       "-P, --parallel <parallelism>	parallel measurement with parallelism threads\n"
	       "-N, --iteration <N>		iteration count of the test. The output result will be\n"
	       "				the average of N iterations\n"
	       "-h, --thp			enable Transparent Huge Pages (THP) for memory pages\n"
	       "-r, --random			walk the circular list in random order. Apply to the\n"
	       "				latency measurement only\n"
	       "-v, --version			show the version of this tool\n"
	       "--latency			measure the memory access latency\n"
	       "--bandwidth			measure the memory access bandwidth. If both --latency\n"
	       "				and --bandwidth are specified or none of them is specified,\n"
	       "				both latency and bandwidth will be measured\n"
	       "--verbose			show verbose information of measurement\n"
	       "--help				show this informaton\n",
	       WAYCA_MEMORY_BENCH);
}

static int parse_command(int argc, char *argv[])
{
	static struct option options[] = {
		{ "length",	required_argument,	NULL, 'l' },
		{ "initiator",	required_argument,	NULL, 'i' },
		{ "target",	required_argument,	NULL, 't' },
		{ "stride",	required_argument,	NULL, 's' },
		{ "parallel",	required_argument,	NULL, 'P' },
		{ "iteration",	required_argument,	NULL, 'N' },
		{ "thp",	no_argument,		NULL, 'h' },
		{ "random",	no_argument,		NULL, 'r' },
		{ "version",	no_argument,		NULL, 'v' },
		{ "latency",	no_argument,		NULL,  0  },
		{ "bandwidth",	no_argument,		NULL,  0  },
		{ "verbose",	no_argument,		NULL,  0  },
		{ "help",	no_argument,		NULL,  0  },
		{ 0,		0,			0,     0  }
	};
	int c, opt_idx, len;
	char *endptr;

	while ((c = getopt_long(argc, argv, "l:hi:t:rs:P:N:v", options, &opt_idx)) != EOF) {
		switch (c) {
		case 0:
			if (!strcmp(options[opt_idx].name, "latency")) {
				lat_bench = true;
			} else if (!strcmp(options[opt_idx].name, "bandwidth")) {
				bw_bench = true;
			} else if (!strcmp(options[opt_idx].name, "help")) {
				usage();
				return 0;
			} else if (!strcmp(options[opt_idx].name, "verbose")) {
				verbose = true;
			}
			break;
		case 'l':
			len = strlen(optarg);
			endptr = isdigit(optarg[len - 1]) ? NULL : optarg + len - 1;
			buf_sz = strtoul(optarg, &endptr, 0);
			if (buf_sz == ULONG_MAX) {
				perror("invalid memory size\n");
				return -1;
			} else if (tolower(optarg[len - 1]) == 'k') {
				buf_sz *= KiB;
			} else if (tolower(optarg[len - 1]) == 'm') {
				buf_sz *= MiB;
			} else if (tolower(optarg[len - 1]) == 'g') {
				buf_sz *= GiB;
			}

			break;
		case 'h':
			use_thp = true;
			break;
		case 'i':
			initiator_cpu = strtol(optarg, NULL, 0);
			break;
		case 't':
			target_cpu = strtol(optarg, NULL, 0);
			break;
		case 'r':
			random_access = true;
			break;
		case 's':
			stride = strtol(optarg, NULL, 0);
			/* stride should be LATENCY_TYPE aligned and not zero */
			if (stride % sizeof(LATENCY_TYPE) || stride == 0) {
				fprintf(stderr, "stride should be %ld aligned\n",
					sizeof(LATENCY_TYPE));
				return -EINVAL;
			}

			break;
		case 'P':
			parallel = strtol(optarg, NULL, 0);
			if (parallel < 0 || parallel == LONG_MAX) {
				perror("invalid parallel number\n");
				return -EINVAL;
			}
			break;
		case 'N':
			iteration = strtol(optarg, NULL, 0);
			if (iteration <= 0 || parallel == LONG_MAX) {
				perror("invalid iteration number\n");
				return -EINVAL;
			}
			break;
		case 'v':
			show_version();
			return 0;
		default:
			usage();
			return -EINVAL;
		}
	}

	/* Perform both measurement if none specified */
	if (!lat_bench && !bw_bench) {
		lat_bench = true;
		bw_bench = true;
	}

	/* We need at least one bench thread */
	if (!parallel) {
		parallel = 1;
	}

	/*
	 * User specifies parallel number of bench threads, add 1 to
	 * include the allocation thread.
	 */
	parallel += 1;

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	ret = parse_command(argc, argv);
	if (ret)
		return ret;

	init_bench_parameters();

	if (lat_bench)
		ret = measure_memory_latency();
	if (bw_bench && !ret)
		ret = measure_memory_bandwidth();

	return ret;
}

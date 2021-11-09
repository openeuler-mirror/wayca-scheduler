#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include <wayca-scheduler.h>

wayca_sc_threadpool_t wayca_threadpool;
static pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;
long total_queue_time = 0;

struct threadinfo {
	int index;
	struct timeval begin;
};

struct threadinfo *info;

void task_func(void *priv)
{
	struct threadinfo *this_info = priv;
	struct timeval end;
	long time;

	gettimeofday(&end, NULL);
	time = (end.tv_sec - this_info->begin.tv_sec) * 1000 + (end.tv_usec - this_info->begin.tv_usec);
	pthread_mutex_lock(&time_mutex);
	total_queue_time += time;
	pthread_mutex_unlock(&time_mutex);

	printf("Task %d finished in %.12f sec\n", this_info->index, (float)time / 1000);
}

int main(int argc, char *argv[])
{
	int thread_num = 0, task_num = 0, ret, c;
	static struct option options[] = {
		{ "thread", required_argument, NULL, 't' },
		{ "tasks", required_argument, NULL, 'T' },
		{ 0, 0, 0, 0 },
	};

	while ((c = getopt_long(argc, argv, "t:T:", options, NULL)) != -1) {
		switch (c) {
		case 't':
			thread_num = atoi(optarg);
			break;
		case 'T':
			task_num = atoi(optarg);
			break;
		}
	}

	if (!thread_num)
		thread_num = sysconf(_SC_NPROCESSORS_CONF);
	if (!task_num)
		task_num = thread_num * 100;

	printf("thread_num %d, task_num %d\n", thread_num, task_num);

	info = malloc(task_num * sizeof(struct threadinfo));
	if (!info)
		return -ENOMEM;

	ret = wayca_sc_threadpool_create(&wayca_threadpool, thread_num);
	if (ret <= 0)
		return ret;

	for (int i = 0; i < task_num; i++) {
		printf("Queue Task %d\n", i);
		info[i].index = i;
		gettimeofday(&info[i].begin, NULL);
		ret = wayca_sc_threadpool_queue(wayca_threadpool, task_func, &info[i]);
		if (ret)
			break;
	}

	/* Wait for all the tasks finished */
	while (wayca_sc_threadpool_running_num(wayca_threadpool))
		sched_yield();

	wayca_sc_threadpool_destroy(wayca_threadpool);

	printf("Average queue time is %.12f\n", (float)total_queue_time / task_num / 1000);
	return 0;
}

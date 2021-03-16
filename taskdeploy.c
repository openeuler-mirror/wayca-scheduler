#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <wayca-scheduler.h>

#include "lib/common.h"

int perf_stat(pid_t pid, int start);

static void usage(void)
{

	printf("usage:\n\n");
	printf("#bind all threads of process 1000 to cpu0-3\n");
	printf("taskdeploy --pid 1000 --cpu 0-3 --all\n\n");

	printf("#bind thread 1000 to cpu0 or cpu3\n");
	printf("taskdeploy --pid 1000 --cpu 0,3\n\n");

	printf("#bind thread 1000 to cpu0 or cpu3\n");
	printf("taskdeploy --pid 1000 --cpu 0,3\n\n");

	printf("#bind all threads of process 1000 to cpu0-3 and migrate pages to node0\n");
	printf("taskdeploy --pid 1000 --cpu 0-3 --all --mem 0\n");

	printf("#execute a.out on CPU0 and memory node0\n");
	printf("taskdeploy --exe --cpu 0 --mem 0 ./a.out\n\n");
}

static void version(void)
{
	printf("wayca-taskdeploy in wayca-deployer toolset:%s\n",
	       WAYCA_DEPLOY_VERSION);
}

static void parse_command_line(int argc, char **argv)
{
	int opt;
	bool all_threads = false;
	bool exec = false;
	pid_t pid = -1;
	int mem_node = -1;
	char cpulist[200] = { 0 };
	int ret;

	static struct option const lopts[] = {
		{"pid", 1, NULL, 'p'},
		{"exe", 0, NULL, 'e'},
		{"cpu", 1, NULL, 'c'},
		{"mem", 1, NULL, 'm'},
		{"all", 0, NULL, 'a'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "ap:hVc:m:e", lopts, NULL)) != -1) {

		switch (opt) {
		case 'h':
			usage();
			exit(0);
			break;
		case 'V':
			version();
			exit(0);
			break;
		case 'c':
			snprintf(cpulist, sizeof(cpulist), "%s", optarg);
			break;
		case 'p':
			pid = strtoul(optarg, NULL, 10);
			break;
		case 'm':
			mem_node = strtoul(optarg, NULL, 10);
			break;
		case 'a':
			all_threads = true;
			break;
		case 'e':
			exec = true;
			break;
		}
	}

	if (exec)
		pid = 0;

	if (!strlen(cpulist) || pid == -1) {
		usage();
		exit(1);
	}

	if (pid != 0) {
		perf_stat(pid, 1);

		if (all_threads && pid)
			ret = process_bind_cpulist(pid, cpulist);
		else
			ret = thread_bind_cpulist(pid, cpulist);

		if (ret < 0)
			fprintf(stderr,
				"Cannot change task(s) %d's affinity to cpulist %s\n",
				pid, cpulist);
		else
			printf("Changed task(s) %d's affinity to cpu %s\n", pid,
			       cpulist);

		if (mem_node != -1) {
			ret = mem_migrate_to_node(pid, mem_node);
			if (ret < 0)
				fprintf(stderr,
					"Cannot migrate task(s) %d's pages to node %d\n",
					pid, mem_node);
			else
				printf("Migrated task(s) %d's pages to node %d\n",
				       pid, mem_node);
		}

		/* sleep 1 second to the new deployment stable
		 * so that the profiling data is correct
		 */
		sleep(1);

		perf_stat(pid, 0);
	} else {
		argv += optind;
		thread_bind_cpulist(pid, cpulist);
		if (mem_node != -1)
			mem_bind_node(mem_node);

		printf("starting app %s on cpu%s memory%d\n", argv[0], cpulist,
		       mem_node);
		execvp(argv[0], argv);
	}
}

int main(int argc, char **argv)
{
	parse_command_line(argc, argv);

	return 0;
}

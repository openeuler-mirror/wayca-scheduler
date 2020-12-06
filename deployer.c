#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/limits.h>

#include <waycadeployer.h>

#include "common.h"

static int socket_fd = -1;

static int start_program(struct program *prog, char **argv)
{
	char managed_threads[PATH_MAX + 20] = { 0 };
	char *env[] = { managed_threads, (char *)NULL };
	int n = strlen(prog->cpu_list);
	int flags;

	printf("Starting %s on cpu:%s util:%d io_node:%d mem bandwidth:%d\n",
	       prog->exec,
	       prog->task_bind_mode == AUTO ? "auto" : prog->cpu_list,
	       prog->cpu_util, prog->io_node, prog->mem_band);

	prog->pid = getpid();
	/* Memory */
	if (prog->mem_band == PACKAGE)
		mem_interleave_in_package(prog->io_node);
	else if (prog->mem_band == ALL)
		mem_interleave_in_all();

	if (socket_fd != -1) {
		int ret;
		write(socket_fd, prog, sizeof(*prog));
		ret = read(socket_fd, &flags, sizeof(flags));
		if (ret <= 0)
			fprintf(stderr, "Failed to deploy %s by deployd\n",
				prog->exec);
	}

	if (n > 0 && prog->task_bind_mode == FINE)
		sprintf(managed_threads, "MANAGED_THREADS=%s", prog->cpu_list);

	if (argv[0])
		execvpe(argv[0], argv, env);
	else
		execle("/bin/sh", "/bin/sh", "-c", prog->exec, NULL, env);

	_exit(-1);

	return 0;
}

static int cfg_strtoul(char *buf)
{
	char *p = strchr(buf, '=');
	if (!p)
		return -1;

	return strtoul(p + 1, NULL, 10);
}

static int cfg_strtopair(char *buf, int pair[][2])
{
	char *p;
	char *q;
	int i = 0;

	p = strchr(buf, '=');
	if (!p)
		return -1;

	while (p) {
		p = p + 1;
		q = strchr(p, '@');
		q = q + 1;

		pair[i][0] = strtoul(p, NULL, 10);
		pair[i][1] = strtoul(q, NULL, 10);
		i++;

		p = strchr(q, ' ');
	};

	return 0;
}

static int parse_cfg_and_run(char *path, char **argv)
{
	FILE *fp;
	char buf[PATH_MAX];
	struct program *prog = NULL;

	fp = fopen(path, "r");

	/*
	 * PROG section, deploy applications based on the characteristics of each program
	 */
	while (fgets(buf, sizeof(buf), fp)) {
		char *p = buf;
		if (str_start_with(p, "[PROG]")) {
			if (prog) {
				fprintf(stderr,
					"parse_cfg_and_run: duplicated [PROG] section\n");
				free(prog);
				return -1;
			}
			prog = malloc(sizeof(*prog));
			if (!prog) {
				fprintf(stderr,
					"parse_cfg_and_run: failed to allocate memory\n");
				return -1;
			}
			memset(prog->exec, 0, sizeof(prog->exec));
			memset(prog->irq_bind, -1, sizeof(prog->irq_bind));
			prog->io_node = prog->cpu_util = -1;
			prog->mem_band = -1;
			prog->cpu_list[0] = '\0';
		}

		if (str_start_with(p, "[/PROG]")) {
			if (!prog) {
				fprintf(stderr,
					"parse_cfg_and_run: lacking [PROG] section\n");
				return -1;
			}
			break;
		}

		if (!prog)
			continue;
		if (str_start_with(p, "exec"))
			cfg_strtostr(p, prog->exec);
		else if (str_start_with(p, "cpu_util"))
			prog->cpu_util = cfg_strtoul(p);
		else if (str_start_with(p, "io_node"))
			prog->io_node = cfg_strtoul(p);
		else if (str_start_with(p, "task_bind")) {
			if (strstr(p, "AUTO"))
				prog->task_bind_mode = AUTO;
			else {
				cfg_strtostr(p, prog->cpu_list);
				if (strstr(prog->cpu_list, "@"))
					prog->task_bind_mode = FINE;
				else
					prog->task_bind_mode = COARSE;
			}
		} else if (str_start_with(p, "irq_bind")) {
			cfg_strtopair(buf, prog->irq_bind);
		} else if (str_start_with(p, "mem_bandwidth")) {
			char mem_band[PATH_MAX];
			cfg_strtostr(p, mem_band);
			if (!strcmp(mem_band, "LOW"))
				prog->mem_band = LOW;
			else if (!strcmp(mem_band, "DIE"))
				prog->mem_band = DIE;
			else if (!strcmp(mem_band, "PACKAGE"))
				prog->mem_band = PACKAGE;
			else if (!strcmp(mem_band, "ALL"))
				prog->mem_band = ALL;
		};
	}

	if (prog) {
		argv += optind;
		start_program(prog, argv);
		free(prog);
	}

	fclose(fp);
	return 0;
}

static void usage(void)
{
	printf("Usage:\n");
	printf("#deploy a program by a configuration file\n");
	printf("wayca-deployer --file deploy.cfg /usr/bin/prog\n\n");
}

static void version(void)
{
	printf("wayca deployer v0.1\n");
}

static void parse_command_line(int argc, char **argv)
{
	int opt;
	bool cfg = false;

	static struct option const lopts[] = {
		{"file", 1, NULL, 'f'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "hVf:", lopts, NULL)) != -1) {

		switch (opt) {
		case 'h':
			usage();
			exit(0);
			break;
		case 'V':
			version();
			exit(0);
			break;
		case 'f':
			cfg = true;
			parse_cfg_and_run(optarg, argv);
			break;
		}
	}

	if (!cfg)
		usage();
}

static int init_socket()
{
	int ret;

	static struct sockaddr_un srv_addr;

	socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		perror("cannot create communication socket");
		return -1;
	}

	srv_addr.sun_family = AF_UNIX;
	strncpy(srv_addr.sun_path, SOCKET_PATH, sizeof(srv_addr.sun_path));

	ret = connect(socket_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
	if (ret == -1) {
		perror("cannot connect to the server");
		close(socket_fd);

		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	init_socket();
	parse_command_line(argc, argv);

	return 0;
}

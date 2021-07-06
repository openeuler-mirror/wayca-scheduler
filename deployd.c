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
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <wayca-scheduler.h>

#include "common.h"

#define WAYCA_SCD_DEFAULT_CONFIG_PATH "/etc/waycadeployer/deployer.cfg"
static char *config_file_path = WAYCA_SCD_DEFAULT_CONFIG_PATH;

/* default CPU binding modes */
static enum CPUBIND default_task_bind = AUTO;

/* default memory bandwidth requirment of the application */
static enum MEMBAND default_mem_bandwidth = ALL;

#define NR_CPUS 1024

static int ccl_cpus_load[NR_CPUS];
static int node_cpus_load[NR_CPUS];
static int socket_fd;

static int ccl_idle_cpu_cores(int ccl)
{
	return wayca_sc_cpus_in_ccl() - ccl_cpus_load[ccl];
}

static int node_idle_cpu_cores(int node)
{
	return wayca_sc_cpus_in_node() - node_cpus_load[node];
}

static int process_cpulist_bind(struct program *prog)
{
	cpu_set_t mask;

	int cr_in_total = wayca_sc_cpus_in_total();
	int cr_in_ccl = wayca_sc_cpus_in_ccl();
	int cr_in_node = wayca_sc_cpus_in_node();

	list_to_mask(prog->cpu_list, &mask);
	for (int i = 0; i < cr_in_total; i++) {
		if (CPU_ISSET(i, &mask)) {
			ccl_cpus_load[i / cr_in_ccl]++;
			node_cpus_load[i / cr_in_node]++;
		}
	}

	thread_bind_cpulist(prog->pid, prog->cpu_list);
	return 0;
}

static int process_managed_threads_bind(struct program *prog)
{
	char *p = prog->cpu_list;
	int i, j;
	struct task_cpu_map *maps = malloc(sizeof(*maps) * MAX_MANAGED_MAPS);

	if (!maps) {
		fprintf(stderr, "%s failed to allocate memory\n", __func__);
		return -1;
	}

	to_task_cpu_map(p, maps);

	for (i = 0; i < MAX_MANAGED_MAPS; i++) {
		if (CPU_COUNT(&maps[i].tasks) > 0) {
			int nodes = CPU_COUNT(&maps[i].nodes);
			int cpus = CPU_COUNT(&maps[i].cpus);

			if (nodes > 0) {
				for (j = 0; j < wayca_sc_nodes_in_total(); j++) {
					if (NODE_ISSET(j, &maps[i].nodes))
						node_cpus_load[j] += maps[i].cpu_util / nodes;
				}
			} else {
				for (j = 0; j < wayca_sc_cpus_in_total(); j++) {
					if (CPU_ISSET(j, &maps[i].cpus)) {
						node_cpus_load[j / wayca_sc_cpus_in_node()] += maps[i].cpu_util / cpus;
						ccl_cpus_load[j / wayca_sc_cpus_in_ccl()] += maps[i].cpu_util / cpus;
					}
				}
			}
		}
	}

	free(maps);

	return 0;
}

static int occupied_cpu_to_load(char *s)
{
	cpu_set_t mask;

	int cr_in_total = wayca_sc_cpus_in_total();
	int cr_in_ccl = wayca_sc_cpus_in_ccl();
	int cr_in_node = wayca_sc_cpus_in_node();

	list_to_mask(s, &mask);
	for (int i = 0; i < cr_in_total; i++) {
		if (CPU_ISSET(i, &mask)) {
			ccl_cpus_load[i / cr_in_ccl]++;
			node_cpus_load[i / cr_in_node]++;
		}
	}

	return 0;
}

static int process_auto_bind(struct program *prog)
{
	int cr_in_pack = wayca_sc_cpus_in_package();
	int cr_in_ccl = wayca_sc_cpus_in_ccl();
	int cr_in_node = wayca_sc_cpus_in_node();
	int cpu = cr_in_node * prog->io_node;

	if (prog->io_node < 0)
		return 0;

	switch (prog->mem_band) {
		/*
		 * For a process which is not sensitive to memory bandwidth, try to put them in same CCL
		 * if no idle CCL available, put it in same DIE
		 */
	case LOW:
		for (int i = 0; i < cr_in_pack; i += cr_in_ccl) {
			int ccl = (i + cpu) / cr_in_ccl;
			if (ccl_idle_cpu_cores(ccl) >= prog->cpu_util) {
				thread_bind_ccl(prog->pid, i + cpu);
				ccl_cpus_load[ccl] += prog->cpu_util;
				node_cpus_load[(i + cpu) / cr_in_node] +=
				    prog->cpu_util;
				return 0;
			}
		}
	case DIE:
		if (node_idle_cpu_cores(prog->io_node) >= prog->cpu_util) {
			thread_bind_node(prog->pid, prog->io_node);
			node_cpus_load[prog->io_node] += prog->cpu_util;
		} else {
			thread_bind_package(prog->pid, prog->io_node);
		}
		break;
	case PACKAGE:
		thread_bind_package(prog->pid, prog->io_node);
		break;
	case ALL:
		thread_unbind(prog->pid);
		break;
	default:		/* cfg has no memory_bandwidth parameter */
		thread_bind_package(prog->pid, prog->io_node);
		break;
	}

	return 0;
}

static int parse_cfg_file(void)
{
	char buf[PATH_MAX];
	size_t len = 0;
	FILE *fp;

	fp = fopen(config_file_path, "r");
	if (!fp) {
		perror("Failed to open waycadeployer configuration file");
		return -1;
	}

	/*
	 * SYS section, some CPUs might have been bounded by other ways,
	 * exclude them in wayca-deployer
	 */
	if (fgets(buf, sizeof(buf), fp)) {
		char *p = buf;
		if (!str_start_with(p, "[SYS]")) {
			fprintf(stderr,
				"Lacking SYS section,wrong config file");
			fclose(fp);
			return -1;
		}

		len = 0;
		p = NULL;
		while (getline(&p, &len, fp) != EOF) {
			if (str_start_with(p, "occupied_cpus")) {
				char occupied_cpus[PATH_MAX];
				if (cfg_strtostr(p, occupied_cpus) == 0)
					occupied_cpu_to_load(occupied_cpus);
			}
			else if (str_start_with(p, "occupied_io_nodes")) {
				char occupied_io_nodes[PATH_MAX];
				cfg_strtostr(p, occupied_io_nodes);
				/* TODO: not impemented occupied_io_nodes */
			}
			else if (str_start_with(p, "default_task_bind")) {
				char default_task_bind_str[PATH_MAX];
				if (cfg_strtostr(p, default_task_bind_str) == 0) {
					cfg_strtocpubind(default_task_bind_str, &default_task_bind);
					fprintf(stdout, "default task bind is %s\n", cpubind_string[default_task_bind]);
				}
			}
			else if (str_start_with(p, "default_mem_bandwidth")) {
				char default_mem_bandwidth_str[PATH_MAX];
				if (cfg_strtostr(p, default_mem_bandwidth_str) == 0) {
					cfg_strtomemband(default_mem_bandwidth_str, &default_mem_bandwidth);
					fprintf(stdout, "default memory bandwidth is %s\n", memband_string[default_mem_bandwidth]);
				}
			}
			else {	/* unrecognized configuration */
				fprintf(stdout, "WARN: unrecognized configuration line: %s\n", p);

			}
		}
		free(p);
	}

	fclose(fp);
	return 0;
}

static int init_socket()
{
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));

	unlink(SOCKET_PATH);
	umask(0);
	socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		fprintf(stderr, "Failed to create socket\n");
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path));
	if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Failed to bind socket\n");
		return -1;
	}

	listen(socket_fd, 1);
	return 0;
}

static int deploy_program(struct program *prog, int fd)
{
	int i;
	int n = strlen(prog->cpu_list);
	int flags = 1;

	printf("Deploying %s on cpu:%s util:%d io_node:%d mem bandwidth:%d\n",
	       prog->exec,
	       prog->task_bind_mode == AUTO ? "auto" : prog->cpu_list,
	       prog->cpu_util, prog->io_node, prog->mem_band);

	/* bind tasks to CPUs */
	if (prog->task_bind_mode == AUTO)
		/* FIXME: we should do auto bind after we finish coarse and fine bind */
		process_auto_bind(prog);
	else if (prog->task_bind_mode == COARSE && n > 0)
		process_cpulist_bind(prog);
	else if (prog->task_bind_mode == FINE && n > 0)
		process_managed_threads_bind(prog);

	/* IRQ, this should be done by deployed with root permission */
	for (i = 0; i < MAX_IRQS_BIND; i++) {
		if (prog->irq_bind[i][0] != -1)
			wayca_sc_irq_bind_cpu(prog->irq_bind[i][0],
				     prog->irq_bind[i][1]);
	}

	/* tell client deployed has completed the binding */
	write(fd, &flags, sizeof(flags));

	return 0;
}

void parse_command_line(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "f:s:")) != EOF) {
		switch (opt) {
		case 'f':
			config_file_path = strdup(optarg);
			break;
		case 's':
			wayca_scheduler_socket_path = strdup(optarg);
			break;
		default:
			break;
		}
	}
}

int main(int argc, char **argv)
{
	struct sockaddr_un cli_addr;
	int cli_fd, maxfd;
	socklen_t len;
	int client[FD_SETSIZE];
	fd_set rset, allset;
	int ret;
	int i;

	parse_command_line(argc, argv);
	parse_cfg_file();

	ret = init_socket();
	if (ret)
		return -1;

	maxfd = socket_fd;
	for (i = 0; i < FD_SETSIZE; i++)
		client[i] = -1;
	FD_ZERO(&allset);
	FD_SET(socket_fd, &allset);

	while (1) {
		int events;

		rset = allset;
		events = select(maxfd + 1, &rset, NULL, NULL, NULL);
		if (events < 0) {
			perror("Failed to select");
			exit(-1);
		}

		if (FD_ISSET(socket_fd, &rset)) {
			len = sizeof(cli_addr);
			cli_fd =
			    accept(socket_fd, (struct sockaddr *)&cli_addr,
				   &len);

			for (i = 0; i < FD_SETSIZE; i++)
				if (client[i] < 0) {
					client[i] = cli_fd;
					break;
				}
			if (i == FD_SETSIZE) {
				fprintf(stderr, "too many clients\n");
				exit(1);
			}

			FD_SET(cli_fd, &allset);
			if (cli_fd > maxfd)
				maxfd = cli_fd;

			if (--events == 0)
				continue;
		}

		for (i = 0; i < FD_SETSIZE; i++) {
			int fd = client[i];
			if (fd >= 0 && FD_ISSET(fd, &rset)) {
				struct program prog;
				int size = read(fd, &prog, sizeof(prog));
				if (size == 0) {	/* EOF */
					close(fd);
					FD_CLR(fd, &allset);
					client[i] = -1;
				} else {
					deploy_program(&prog, fd);
				}

				if (--events == 0)
					break;
			}
		}
	}

	unlink(SOCKET_PATH);
	free(config_file_path);

	return 0;
}

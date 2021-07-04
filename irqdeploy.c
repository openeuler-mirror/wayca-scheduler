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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <wayca-scheduler.h>

static void usage(void)
{
	printf("usage:\n\n");
	printf("#bind irq 10 to cpu2\n");
	printf("wayca-deployer --irq 10 --cpu 2\n\n");
}

static void version(void)
{
	printf("wayca-irqdeploy in wayca-deployer toolset:%s\n",
	       WAYCA_DEPLOY_VERSION);
}

static void parse_command_line(int argc, char **argv)
{
	int opt;
	int cpu = -1;
	int irq = -1;
	int ret;

	static struct option const lopts[] = {
		{"irq", 1, NULL, 'i'},
		{"cpu", 1, NULL, 'c'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "hVi:c:", lopts, NULL)) != -1) {

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
			cpu = strtoul(optarg, NULL, 10);
			break;
		case 'i':
			irq = strtoul(optarg, NULL, 10);
			break;
		}
	}

	if (cpu == -1 || irq == -1) {
		usage();
		exit(1);
	}

	ret = wayca_sc_irq_bind_cpu(irq, cpu);
	if (ret < 0)
		fprintf(stderr, "Cannot change irq %i's affinity to cpu %i\n",
			irq, cpu);
	else
		printf("Changed irq %i's affinity to cpu %i\n", irq, cpu);
}

int main(int argc, char **argv)
{
	parse_command_line(argc, argv);

	return 0;
}

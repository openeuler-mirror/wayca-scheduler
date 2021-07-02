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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include "wayca_sc_info.h"

static struct option lgopts[] = {
	{"input", required_argument, NULL, 'i'},
	{"output", required_argument, NULL, 'o'},
	{"help", required_argument, NULL, 'h'},
	{0, 0, 0, 0},
};

static struct topo_info_args info_args = {0};

static void
print_usage()
{
	printf("wayca-sc-info [-i,o]"
	       "options:\n"
	       "  -i file, --input file		get topo information from file(XML format)\n"
	       "  -o file, --output file	export topo information from file(XML format)\n"
	       "  -h, --help			print this message and exit\n");
}

static bool
check_invalid_file_name(char *file_name)
{
	if (strlen(file_name) >= WAYCA_INFO_MAX_FILE_NAME)
		return true;
	return false;
}

static int
parse_args(int argc, char **argv)
{
	int option_index = 0;
	int wayca_opt;

	while ((wayca_opt = getopt_long(argc, argv, "i:o:h", lgopts,
					&option_index)) != EOF) {
		switch (wayca_opt) {
		case 'i':
			if (check_invalid_file_name(optarg)) {
				topo_err("%s: file name too long.", argv[0]);
				print_usage();
				return -EINVAL;
			}
			info_args.has_input_file = true;
			strncpy(info_args.input_file_name, optarg,
					strlen(optarg));
			topo_info("input xml file name: %s.",
					info_args.input_file_name);
			break;
		case 'o':
			if (check_invalid_file_name(optarg)) {
				topo_err("%s: file name too long.", argv[0]);
				print_usage();
				return -EINVAL;
			}
			strncpy(info_args.output_file_name, optarg,
					strlen(optarg));
			info_args.has_output_file = true;
			topo_info("output xml file name: %s.",
					info_args.output_file_name);
			break;
		case 'h':
			print_usage();
			exit(0);
		default:
			print_usage();
			return -EINVAL;
		}
	}
	if (optind < argc) {
		print_usage();
		return -EINVAL;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	xmlDocPtr topo_doc = NULL;
	int ret;

	ret = parse_args(argc, argv);
	if (ret)
		return ret;

	ret = get_topo_info(&info_args, &topo_doc);
	if (ret) {
		topo_err("get topo info fail, ret = %d.", ret);
		return ret;
	}

	ret = validate_topo_info(topo_doc);
	if (ret) {
		topo_err("invalid topo info, ret = %d.", ret);
		goto topo_end;
	}

	ret = put_topo_info(&info_args, topo_doc);
	if (ret) {
		topo_err("output topo info fail, ret = %d.", ret);
		goto topo_end;
	}

topo_end:
	xmlFreeDoc(topo_doc);
	xmlCleanupParser();
	xmlMemoryDump();
	return ret;
}

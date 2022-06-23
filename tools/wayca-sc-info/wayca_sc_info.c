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

#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "wayca_sc_info.h"

static struct option lgopts[] = {
	{"input", required_argument, NULL, 'i'},
	{"output", required_argument, NULL, 'o'},
	{"help", no_argument, NULL, 'h'},
	{"irq", no_argument, NULL, 'I'},
	{"device", no_argument, NULL, 'D'},
	{"vebose", no_argument, NULL, 'v'},
	{0, 0, 0, 0},
};

static struct topo_info_args info_args = {0};

static void print_usage(void)
{
	printf("wayca-sc-info [-i,o]"
	       "options:\n"
	       "  -i file, --input file		get topo information from file(XML format)\n"
	       "  -o file, --output file	export topo information to file(XML format).\n"
	       "  -v file, --vebose		export all information.\n"
	       "  -D, --device			export device information.\n"
	       "  -I, --irq			export irq information.\n"
	       "  -h, --help			print this message and exit\n");
}

static int canonicalize_export_filename(const char *filename,
					char *final_filename, size_t size)
{
	char file_basename[WAYCA_INFO_MAX_FILE_NAME] = {};
	char file_dirname[WAYCA_INFO_MAX_FILE_NAME] = {};
	char *dname;
	char *bname;
	char *name;

	strncpy(file_dirname, filename, WAYCA_INFO_MAX_FILE_NAME - 1);
	dname = dirname(file_dirname);
	if (!dname)
		return -EINVAL;
	name = realpath(dname, final_filename);
	if (!name) {
		topo_err("access output directory fail, ret = %d", -errno);
		return -errno;
	}

	if (final_filename[strlen(final_filename) - 1] != '/')
		strncat(final_filename, "/", size - strlen(final_filename));

	strncpy(file_basename, filename, WAYCA_INFO_MAX_FILE_NAME - 1);
	bname = basename(file_basename);
	if (!bname)
		return -EINVAL;
	strncat(final_filename, bname, size - strlen(final_filename));
	return 0;
}

static int parse_file_name(const char *filename, bool is_input)
{
	char *name;
	int ret;

	if (is_input) {
		if (info_args.has_input_file) {
			topo_err("too many input file.");
			return -EINVAL;
		}
		info_args.has_input_file = true;

		name = realpath(filename, info_args.input_file_name);
		if (!name) {
			topo_err("access input file failed, ret = %d.", -errno);
			return -errno;
		}
	} else {
		if (info_args.has_output_file) {
			topo_err("too many output file.");
			return -EINVAL;
		}
		info_args.has_output_file = true;

		if (strlen(filename) >= WAYCA_INFO_MAX_FILE_NAME) {
			topo_err("output file name tool long.");
			return -ENAMETOOLONG;
		}

		ret = canonicalize_export_filename(filename,
					info_args.output_file_name,
					sizeof(info_args.output_file_name));
		if (ret) {
			topo_err("get canonical output file name failed, ret = %d.",
					ret);
			return ret;
		}
	}

	topo_info("%s xml file name: %s.", is_input ? "input" : "output",
			is_input ? info_args.input_file_name :
			info_args.output_file_name);
	return 0;
}

static int parse_args(int argc, char **argv)
{
	int option_index = 0;
	int wayca_opt;
	int ret;

	while ((wayca_opt = getopt_long(argc, argv, ":i:o:hIDv", lgopts,
					&option_index)) != EOF) {
		switch (wayca_opt) {
		case 'i':
		case 'o':
			ret = parse_file_name(optarg, wayca_opt == 'i');
			if (ret)
				return ret;
			break;
		case 'I':
		case 'D':
		case 'v':
			info_args.output_irq = wayca_opt == 'I' ||
					       wayca_opt == 'v';
			info_args.output_dev = wayca_opt == 'D' ||
					       wayca_opt == 'v';
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

int main(int argc, char **argv)
{
	xmlDocPtr topo_doc = NULL;
	int ret;

	wayca_sc_set_log_level(WAYCA_SC_LOG_LEVEL_WARN);

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

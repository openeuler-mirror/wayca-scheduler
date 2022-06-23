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

#ifndef _WAYCA_SC_SC_INFO_H
#define _WAYCA_SC_SC_INFO_H

#include <libxml/parser.h>
#include <libxml/tree.h>
#include "lib/log.h"

#define WAYCA_INFO_MAX_FILE_NAME (PATH_MAX + 1)
#define WAYCA_SC_INFO_DEC_BASE 10
#define WAYCA_SC_INFO_HEX_BASE 16
#define CONTENT_STR_LEN 100

struct topo_info_args {
	bool has_input_file;
	bool has_output_file;
	bool output_irq;
	bool output_dev;
	char input_file_name[WAYCA_INFO_MAX_FILE_NAME];
	char output_file_name[WAYCA_INFO_MAX_FILE_NAME];
};

int get_topo_info(struct topo_info_args *args, xmlDocPtr *topo_doc);
int validate_topo_info(xmlDocPtr topo_doc);
int put_topo_info(struct topo_info_args *args, xmlDocPtr topo_doc);

#define topo_err(fmt, args...) \
		WAYCA_SC_LOG_ERR_NO_TS("wayca_sc_info: %s(): " fmt "\n", \
				__func__, ##args)

#define topo_warn(fmt, args...) \
		WAYCA_SC_LOG_WARN_NO_TS("wayca_sc_info: %s(): " fmt "\n", \
				__func__, ##args)

#define topo_info(fmt, args...) \
		WAYCA_SC_LOG_INFO_NO_TS("wayca_sc_info: %s(): " fmt "\n", \
				__func__, ##args)
#endif

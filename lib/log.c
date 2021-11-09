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

/* log.c
 * Author: Guodong Xu <guodong.xu@linaro.org>
 *	   Yicong Yang <yangyicong@hisilicon.com>
 */

#include "log.h"

/* a globally configurable log level */
WAYCA_SC_LOG_LEVEL wayca_sc_log_level;

void wayca_sc_set_log_level(WAYCA_SC_LOG_LEVEL level)
{
	wayca_sc_log_level = level;
}

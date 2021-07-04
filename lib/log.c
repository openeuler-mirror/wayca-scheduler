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
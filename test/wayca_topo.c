/* wayca_topo.c
 * Author: Guodong Xu <guodong.xu@linaro.org>
 * License: GPLv2
 */

#include <stdio.h>
#include "wayca-scheduler.h"

int main()
{
	topo_print();

	printf("cores_in_ccl: %d\n", cores_in_ccl());
	printf("cores_in_node: %d\n", cores_in_node());
	printf("cores_in_package: %d\n", cores_in_package());
	printf("cores_in_total: %d\n", cores_in_total());
	printf("nodes_in_package: %d\n", nodes_in_package());
	printf("nodes_in_total: %d\n", nodes_in_total());

	return 0;
}
/* helper functions
 * Author: Guodong Xu <guodong.xu@linaro.org>
 */

#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CORES_PER_CCL (4)

void print_spreading_cpus(unsigned int n_jobs, unsigned int n_cores, unsigned int offset, const char * delimit)
{
	int i;

	if (n_jobs > n_cores) {
		printf("ERROR");
		return;
	}

	for (i = 0; i < n_jobs; i++) {
		if (i != 0) printf("%s", delimit);
		printf("%d", (int)((double)n_cores / n_jobs * i) + offset);
	}
	return;
}

typedef enum {
	spreading_across_ccls = 0,
	spreading_across_numa = 1,
	spreading_others = -1
} spreading_type;

typedef enum {
	n_8_ccls = 8,
	n_6_ccls = 6,
	n_ccls_others = -1
} numa_ccls_type;

/* print_spreading_numas - spreading n_jobs across n_nodes of NUMA nodes
 *  Output: a string of core numbers into stdout
 *  Input:
 * 	- n_ccls: the number of CCLs which are allowed to use on each NUMA node
 * 	- n_ccls_per_node: the total number of CCLs on a NUMA node
 */
void print_spreading_numas(unsigned int n_jobs, unsigned int n_nodes,
			   numa_ccls_type n_ccls_per_node, unsigned int n_ccls, const char * delimit)
{
	int i;
	unsigned int n_jobs_remaining = n_jobs;
	unsigned int n_nodes_remaining = n_nodes;
	unsigned int n_this_jobs;

	/* first, calculate how many jobs to run on this NUMA nodes */
	for (i = 0; i < n_nodes; i++) {
		n_this_jobs = (int)ceil((double)n_jobs_remaining / n_nodes_remaining);
		// DEBUG printf("%d%s", n_this_jobs, delimit);

		/* second, spread n_this_jobs on this NUMA nodes */
		if (i != 0) printf("%s", delimit);
		print_spreading_cpus(n_this_jobs, n_ccls * CORES_PER_CCL, i * n_ccls_per_node * CORES_PER_CCL, delimit);

		// third, calculate how many remaining
		n_jobs_remaining -= n_this_jobs;
		n_nodes_remaining -= 1;
	}
	return;
}




void main(int argc, char * argv[])
{
	int c;
	spreading_type stype = spreading_others;
	numa_ccls_type n_ccls_per_node = n_ccls_others;

	int n_jobs = 0;
	int n_ccls = 0;
	int n_nodes = 0;
	int offset = 0;

	char *usage = "-t <type> -j <number_of_jobs> [-c <number_of_available_ccls>] [-n <number_of_available_numa_nodes> -l <number_of_ccls_per_numa>] [-o <offset>]\n" \
		      "-t 0 -j <number_of_jobs> -c <number_of_available_ccls> [-o <offset>]\n" \
		     " -t 1 -j <number_of_jobs> -n <number_of_available_numa_nodes> -l <number_of_ccls_per_numa> -c <number_of_available_ccls_each_numa> [-o <offset>]\n";

	while (( c = getopt(argc, argv, "t:j:c:n:l:o:")) != EOF) {
		switch(c) {
		case 't':
			stype = atoi(optarg);
			break;
		case 'j':
			n_jobs = atoi(optarg);
			break;
		case 'c':
			n_ccls = atoi(optarg);
			break;
		case 'n':
			n_nodes = atoi(optarg);
			break;
		case 'l':
			n_ccls_per_node = atoi(optarg);
			break;
		case 'o':
			offset = atoi(optarg);
			break;
		default:
			printf("Usage: %s %s\n", argv[0], usage);
			exit(-1);
			break;
		}
	}

	// printf("%d jobs, %d CCLs\n", atoi(argv[1]), atoi(argv[2]));
	switch(stype) {
	case spreading_across_ccls:
		print_spreading_cpus(n_jobs, n_ccls * CORES_PER_CCL, offset, ",");
		break;
	case spreading_across_numa:
		print_spreading_numas(n_jobs, n_nodes, n_ccls_per_node, n_ccls, ",");
		break;
	default:
		printf("Usage: %s %s\n", argv[0], usage);
		exit(-1);
		break;
	}

	return;
}

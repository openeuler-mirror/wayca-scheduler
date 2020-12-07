#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_PERF_COUNT 30
#define MAX_NAME_LEN   40
struct perf_count {
	char name[MAX_NAME_LEN];
	long long value;
} old[MAX_PERF_COUNT], new[MAX_PERF_COUNT];

/*
 * TODO: move to perf_event_open() and get counter from kernel
 * rather than depending on perf tool
 */
int perf_stat(pid_t pid, int start)
{
	int p[2];
	pid_t cpid;
#define MAX_LEN 400
	char cmd[MAX_LEN];
	sprintf(cmd, "perf stat -e branch-misses,bus-cycles,cache-misses,"
		"cycles,instructions,stalled-cycles-backend,stalled-cycles-frontend,"
		"bus_cycles,mem_access,remote_access,ll_cache,ll_cache_miss"
		" -a -p %d -x '	' -- sleep 5", pid);

	if (pipe(p) == -1) {
		perror("cannot create the IPC pipe");
		return -1;
	}

	cpid = fork();
	if (cpid == -1) {
		perror("cannot create new process");
		return -1;
	} else if (cpid == 0) {
		dup2(p[1], STDERR_FILENO);

		close(p[0]);
		close(p[1]);

		execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
		return 0;
	} else {
		int status;
		int i, j;
		dup2(p[0], STDIN_FILENO);
		close(p[0]);
		close(p[1]);

		do {
			waitpid(cpid, &status, WUNTRACED | WCONTINUED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));

		for (i = 0; i < MAX_PERF_COUNT;) {
			int ret;
			struct perf_count *perf;

			if (start)
				perf = old;
			else
				perf = new;
			ret =
			    scanf("%lld%30s%*[^\n]%*c", &perf[i].value,
				  perf[i].name);
			if (ret >= 2) {
				i++;
			} else if (ret == 0) {
				scanf("%*[^\n]%*c");
			} else if (ret == -1) {
				clearerr(stdin);
				break;
			}
		}

		if (!start) {
			printf
			    ("------[Performance changes after deployment]--------------\n");
			for (j = 0; j < i; j++) {
				float old_ipc, new_ipc;
				printf("%-30s %16lld   -> %16lld %f%%\n",
				       old[j].name, old[j].value, new[j].value,
				       (new[j].value -
					old[j].value) * 100.0 / old[j].value);
				if (strstr(old[j].name, "instructions")) {
					old_ipc =
					    old[j].value * 1.0 / old[j -
								     1].value;
					new_ipc =
					    new[j].value * 1.0 / new[j -
								     1].value;
					printf
					    ("%-30s          %.4f   ->           %.4f %f%%\n",
					     "inst per cycle", old_ipc, new_ipc,
					     (new_ipc -
					      old_ipc) * 100.0 / old_ipc);
					j++;	/* skip stalled cycle */
				}
			}
			printf("------[End]--------------\n");
		}

		return 0;
	}
}

#!/usr/bin/python
#
# Author: Yicong Yang <young.yicong@outlook.com>
# Copyright (c) 2021 HiSilicon Technologies Co., Ltd.
#
# Wayca scheduler is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

from time import sleep
from bcc import BPF
import argparse

parser = argparse.ArgumentParser()

parser.add_argument("-f", "--cpufrom", type = int, help = "the cpu where the migration started from", required = True)
parser.add_argument("-t", "--cputo", type = int, help = "the cpu where the migration targeted to", required = True)
parser.add_argument("-p", "--pid", type = int, help = "the migration task's pid", required = True)

args = parser.parse_args()

cpu_from = args.cpufrom
cpu_to = args.cputo
pid = args.pid

bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>
#include <linux/types.h>

struct stats {
	u64 last;
	u64 total;
	u64 counts;
	u64 prev_cpu;
};

BPF_HASH(stat, int, struct stats, 1);

int trace_ts(struct pt_regs *ctx, struct task_struct *prev)
{
	int cpu = bpf_get_smp_processor_id(), key = 0, cpu_from, cpu_to;
	u64 ts = bpf_ktime_get_ns(), delta;
	pid_t prev_pid, cur_pid;
	struct stats *stat_p;

	cur_pid = bpf_get_current_pid_tgid() & 0xffff;
	prev_pid = prev->pid;
	cpu_from = FROM;
	cpu_to = TO;

	if (cur_pid != PID)
		return 0;

	ts = bpf_ktime_get_ns();
	stat_p = stat.lookup(&key);
	if (!stat_p) {
		struct stats init = {
			.last		= ts,
			.total		= 0,
			.counts		= 0,
			.prev_cpu	= cpu,
		};
		stat.update(&key, &init);
		return 0;
	}

	if (cpu == cpu_from) {
		stat_p->last = ts;
	} else if (cpu == cpu_to && cpu != stat_p->prev_cpu) {
		delta = ts - stat_p->last;
		stat_p->total += delta;
		stat_p->counts++;
	}

	stat_p->prev_cpu = cpu;
	return 0;
}

"""

bpf_text = bpf_text.replace("FROM", "%d" % (cpu_from))
bpf_text = bpf_text.replace("TO", "%d" % (cpu_to))
bpf_text = bpf_text.replace("PID", "%d" % (pid))

b = BPF(text = bpf_text)
b.attach_kprobe(event="finish_task_switch", fn_name="trace_ts")
print("Start tracing...")

exiting = 0
while True:
	try:
		sleep(999999)
	except KeyboardInterrupt:
		exiting = 1

	if exiting:
		print("Exiting...")

		for key, value in b["stat"].items():
			if not value.counts:
				break

			print("Total %d migrations from %d -> %d, average cost %f ns" % (value.counts, cpu_from, cpu_to, value.total / value.counts))

		exit()

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 HiSilicon Limited.
 */

#include <linux/completion.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/sched/task.h>

#if	1
#define priv_info(fmt, ...)	\
	trace_printk(fmt, ## __VA_ARGS__)
#else
#define priv_info(fmt, ...)	\
	pr_info(fmt, ## __VA_ARGS__)
#endif

static int from = 0;
static int to = 0;
static int counts = 1000;
module_param(from, int, 0600);
module_param(to, int, 0600);
module_param(counts, int, 0600);

struct latency_data {
	struct completion complete;
	ktime_t total_cost;
	ktime_t exit;
	int pre_cpu;
};

static int migration_cost_func(void *priv)
{
	struct task_struct *this = current;
	struct latency_data *data = priv;
	cpumask_t _from, _to;
	int nrsw = counts * 2;
	int cpu;

	cpumask_clear(&_from);
	cpumask_clear(&_to);
	cpumask_set_cpu(from, &_from);
	cpumask_set_cpu(to, &_to);

	while (!kthread_should_stop() && nrsw >= 0) {
		cpu = smp_processor_id();

		if (cpu == data->pre_cpu)
			continue;
		data->pre_cpu = cpu;

		if (cpu == from)
			data->exit = ktime_get();
		else
			data->total_cost = ktime_add(data->total_cost, ktime_sub(ktime_get(), data->exit));

		set_cpus_allowed_ptr(this, cpu == from ? &_to : &_from);
		nrsw--;
	}

	complete(&data->complete);
	return 0;
}

static int __init migration_cost_init(void)
{
	struct latency_data data = { 0 };
	struct task_struct *task;
	int ret, i;

	data.pre_cpu = -1;
	init_completion(&data.complete);

	if (!cpumask_test_cpu(from, cpu_online_mask) ||
	    !cpumask_test_cpu(to, cpu_online_mask) ||
	    from == to || i < 0)
		return -EINVAL;

	task = kthread_create_on_node(migration_cost_func, &data, cpu_to_node(from), "migration_cost");
	if (IS_ERR(task))
		return PTR_ERR(task);

	get_task_struct(task);
	kthread_bind(task, from);
	wake_up_process(task);

	wait_for_completion_timeout(&data.complete, 100 * HZ);

	ret = kthread_stop(task);
	if (ret)
		goto out;

	priv_info("from %d to %d count %d total %lluns avg %lluns\n",
		     from, to, counts, ktime_to_ns(data.total_cost), ktime_to_ns(data.total_cost) / counts);

out:
	put_task_struct(task);
	return ret ? ret : -EAGAIN;
}

static void __exit migration_cost_exit(void)
{
	return;
}

module_init(migration_cost_init);
module_exit(migration_cost_exit);

MODULE_AUTHOR("Yicong Yang <young.yicong@outlook.com");
MODULE_DESCRIPTION("Microbenchmark for migration cost between cpus");
MODULE_LICENSE("GPL");

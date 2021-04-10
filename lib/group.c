#define _GNU_SOURCE
#include <assert.h>
#include <string.h>
#include <sched.h>

#include "wayca_thread.h"

bool is_thread_in_group(struct wayca_group *group, struct wayca_thread *thread)
{
	struct wayca_thread *member = group->threads;

	while (member) {
		if (member == thread)
			return true;

		member = member->siblings;
	}

	return false;
}

bool is_group_in_father(struct wayca_group *group, struct wayca_group *father)
{
	struct wayca_group *member = father->groups;

	while (member) {
		if (member == group)
			return true;

		member = member->siblings;
	}

	return false;
}

void group_thread_add_to_tail(struct wayca_group *group, struct wayca_thread *thread)
{
	struct wayca_thread *tail = group->threads;

	assert(thread->siblings == NULL);

	/* No tail thread found, this is an empty group */
	if (!tail) {
		group->threads = thread;
		return;
	}

	while (tail->siblings)
		tail = tail->siblings;

	tail->siblings = thread;
}

void group_group_add_to_tail(struct wayca_group *group, struct wayca_group *father)
{
	struct wayca_group *tail = father->groups;

	assert(group->siblings == NULL);

	if (!tail) {
		father->groups = group;
		return;
	}

	while (tail->siblings)
		tail = tail->siblings;

	tail->siblings = group;
}

void group_thread_delete_thread(struct wayca_group *group, struct wayca_thread *thread)
{
	struct wayca_thread *member = group->threads, *next;

	/**
	 * If the first element is the target thread.
	 */
	if (member == thread) {
		group->threads = thread->siblings;
		thread->siblings = NULL;
		return;
	}

	while (member) {
		next = member->siblings;

		/* Found the target thread */
		if (next == thread) {
			next = next->siblings;
			member->siblings = next;
			thread->siblings = NULL;
			break;
		}

		member = next;
	}
}

void group_group_delete_group(struct wayca_group *group, struct wayca_group *father)
{
	struct wayca_group *member = father->groups, *next;

	if (member == group) {
		father->groups = group->siblings;
		group->siblings = NULL;
		return;
	}

	while (member) {
		next = member->siblings;

		if (next == group) {
			next = next->siblings;
			member->siblings = next;
			group->siblings = NULL;
			break;
		}

		member = next;
	}
}

int wayca_group_request_resource_from_father(struct wayca_group *group, cpu_set_t *cpuset)
{
	int cnts = CPU_COUNT(cpuset);
	struct wayca_group *father;
	cpu_set_t available_set;
	size_t target_pos = 0;

	assert(group->father != NULL);
	assert(!CPU_EQUAL(&group->used, &group->total));

	/*
	 * Assume the father group's resource is always set to fixed.
	 */
	assert(group->father->attribute & WT_GF_FIXED);
	father = group->father;

	if (cnts <= 0)
		return -1;

	CPU_ZERO(cpuset);

	if (!(father->attribute & WT_GF_COMPACT))
		target_pos = father->roll_over_cnts % father->nr_cpus_per_topo;

	memset(&available_set, -1, sizeof(cpu_set_t));
	CPU_XOR(&available_set, &available_set, &father->used);
	CPU_AND(&available_set, &available_set, &father->total);

	while (target_pos < sizeof(cpu_set_t) && !CPU_ISSET(target_pos, &available_set))
		target_pos += father->stride;

	while (cnts) {
		while (CPU_ISSET(target_pos, &father->used)) {
			target_pos += father->stride;

			if (target_pos > cpuset_find_last_set(&father->total)) {
				father->roll_over_cnts++;
				if (CPU_EQUAL(&father->used, &father->total))

					CPU_ZERO(&group->used);
				target_pos = father->roll_over_cnts % father->nr_cpus_per_topo;
			}
		}

		CPU_SET(target_pos, &father->used);
		CPU_SET(target_pos, cpuset);
		cnts--;
	}

	return 0;
}

/**
 * wayca_group_request_resource - Determine how many cpu this group need
 *
 * Figure out how many cpus this group needs and get the required cpus
 * and set in the group->total member.
 *
 * If the group->father is NULL, which means this is the top level group,
 * then assign all the cpus in the system to the group.
 *
 * If the group->threads is NULL, which means this is an empty group, then
 * assign the minimum cpus to the group according to the group's attribute.
 */
int wayca_group_request_resource(struct wayca_group *group)
{
	int nr_threads = group->nr_threads ? group->nr_threads : 4;
	cpu_set_t required_cpuset;

	if (group->father == NULL) {
		memcpy(&group->total, &total_cpu_set, sizeof(cpu_set_t));
		return 0;
	}

	CPU_ZERO(&required_cpuset);

	/**
	 * Setup the required cpuset numbers. No position information
	 * is included. The father will pass assigned cpuset later
	 * with the position of the start cpu. If the group has no father,
	 * the required set will always start from 0.
	 */
	for (int pos = 0; pos < nr_threads; pos++) {
		int cpu = pos * group->stride;
		CPU_SET(cpu, &required_cpuset);
	}

	/**
	 * TBD:
	 * 	If cpuset required from father failed, we need some fallback
	 * 	process rather than directly return fail.
	 */
	if (group->father != NULL &&
	    wayca_group_request_resource_from_father(group, &required_cpuset))
		return -1;

	memcpy(&group->total, &required_cpuset, sizeof(cpu_set_t));
	return 0;
}

int wayca_group_init(struct wayca_group *group)
{
	/* Init with no members */
	group->threads = NULL;
	group->nr_threads = 0;

	/* Init group in no hierarchy */
	group->siblings = NULL;
	group->father = NULL;
	group->topo_hint = -1;
	group->roll_over_cnts = 0;

	CPU_ZERO(&group->used);

	/* Init the group attribute, threads will compact in CCL and bind per-CPU */
	group->attribute = (WT_GF_CCL | WT_GF_COMPACT | WT_GF_PERCPU);

	return wayca_group_arrange(group);
}

int wayca_group_arrange(struct wayca_group *group)
{
	/* Arrange the parameters according to the attribute */
	switch (group->attribute & 0xffff) {
	case WT_GF_CPU:
		group->nr_cpus_per_topo = 1;
		break;
	case WT_GF_CCL:
		group->nr_cpus_per_topo = cores_in_ccl();
		break;
	case WT_GF_NUMA:
		group->nr_cpus_per_topo = cores_in_node();
		break;
	case WT_GF_PACKAGE:
		group->nr_cpus_per_topo = cores_in_package();
		break;
	case WT_GF_ALL:
		group->nr_cpus_per_topo = cores_in_total();
		break;
	}

	/**
	 * Check the relationship of the threads in this group,
	 * and assign the proper stride to emplace each thread.
	 */
	if (group->attribute & WT_GF_COMPACT)
		group->stride = 1;
	else
		group->stride = group->nr_cpus_per_topo;

	return wayca_group_request_resource(group);
}

int wayca_group_assign_thread_resource(struct wayca_group *group, struct wayca_thread *thread)
{
	cpu_set_t available_set;
	size_t target_pos = 0;

	/* Scatter case */
	if (!(group->attribute & WT_GF_COMPACT))
		target_pos = group->roll_over_cnts % group->nr_cpus_per_topo;

	memset(&available_set, -1, sizeof(cpu_set_t));
	CPU_XOR(&available_set, &available_set, &group->used);
	CPU_AND(&available_set, &available_set, &group->total);

	/* iterate the available cpu set and find a proper cpu */
	while(target_pos < sizeof(cpu_set_t) && !CPU_ISSET(target_pos, &available_set))
		target_pos += group->stride;

	/**
	 * We assume all the threads in this group apply the same schedule
	 * method: pinned on per CPU or freely scheduled in the desired
	 * cpuset. Judge the group->stride here to retrieve the schedule
	 * method and assigned the cpu to the thread.
	 */

	/* Reset the thread's cpuset infomation first */
	CPU_ZERO(&thread->allowed_set);
	CPU_ZERO(&thread->cur_set);

	/**
	 * If the bind policy is per-CPU, then
	 * we only to assign one single CPU to the thread. So we directly
	 * assign the CPU at the @target_pos to the thread.
	 *
	 * Otherwise, we assign a set of CPUs to the thread, ranged
	 * [target_pos, target_pos + group->nr_cpus_per_topo).
	 */
	if (group->attribute & WT_GF_PERCPU) {
		CPU_SET(target_pos, &thread->cur_set);
		CPU_SET(target_pos, &thread->allowed_set);
	} else {
		/**
		 * If the bind policy is not per-CPU, then each thread will
		 * bind to a set of CPU according to the topology level.
		 * So always make the start CPU at the beginning of the
		 * The topology set to avoid intercrossing between adjacent
		 * sets.
		 */
		int anchor = target_pos - target_pos % group->nr_cpus_per_topo;
		for (int num = anchor;
		     num < anchor + group->nr_cpus_per_topo;
		     num++)
		{
			CPU_SET(num, &thread->cur_set);
			CPU_SET(num, &thread->allowed_set);
		}
	}

	/**
	 * Update the group's resource information.
	 * If the relationship between threads is compact, we only
	 * record the @target_pos of this thread in the group->used,
	 * as next thread may be placed in the same affinity set.
	 * Otherwise the threads are scattered, we have to record
	 * all the CPUs assigned to this thread in the group->used,
	 * as it's exclusive to the next thread.
	 */
	if (group->attribute & WT_GF_COMPACT)
		CPU_SET(target_pos, &group->used);
	else
		CPU_OR(&group->used, &group->used, &thread->allowed_set);

	/**
	 * We have to hanlde the roll over case when the group's
	 * resources cannot be adjusted according to the threads
	 * it owns.
	 */
	if (group->attribute & WT_GF_FIXED)
	{
		if (CPU_EQUAL(&group->used, &group->total)) {
			group->roll_over_cnts++;
			CPU_ZERO(&group->used);
		} else if (target_pos + group->stride >= cpuset_find_last_set(&group->total)) {
			group->roll_over_cnts++;
		}
	}

	return 0;
}

int wayca_group_add_thread(struct wayca_group *group, struct wayca_thread *thread)
{
	cpu_set_t available_set;
	size_t available_cnt;

	memset(&available_set, -1, sizeof(cpu_set_t));
	CPU_XOR(&available_set, &available_set, &group->used);
	CPU_AND(&available_set, &available_set, &group->total);
	available_cnt = CPU_COUNT(&available_set);

	/**
	 * TBD:
	 * 	If there is no availabe CPU and the policy of the
	 * 	group is not fixed, we first require resources
	 * 	from the parent group.
	 */
	if (!available_cnt && (group->attribute & WT_GF_FIXED))
		;

	group->nr_threads++;
	group_thread_add_to_tail(group, thread);

	thread->group = group;

	wayca_group_assign_thread_resource(group, thread);

	return 0;
}

int wayca_group_delete_thread(struct wayca_group *group, struct wayca_thread *thread)
{
	if (!is_thread_in_group(group, thread))
		return -1;

	if (CPU_COUNT(&group->used) == 0) {
		assert(group->roll_over_cnts > 0);

		group->roll_over_cnts--;
		CPU_OR(&group->used, &group->used, &group->total);
	}

	CPU_XOR(&group->used, &group->used, &thread->allowed_set);

	group_thread_delete_thread(group, thread);
	thread->group = NULL;
	group->nr_threads--;

	return 0;
}

int wayca_group_rearrange_thread(struct wayca_group *group, struct wayca_thread *thread)
{
	thread_sched_setaffinity(thread->pid, sizeof(cpu_set_t), &thread->cur_set);

	return 0;
}

int wayca_group_rearrange_group(struct wayca_group *group)
{
	struct wayca_thread *thread;

	if (wayca_group_arrange(group))
		return -1;

	CPU_ZERO(&group->used);
	group->roll_over_cnts = 0;

	group_for_each_threads(thread, group) {
		wayca_group_assign_thread_resource(group, thread);
		wayca_group_rearrange_thread(group, thread);
	}

	return 0;
}

int wayca_group_add_group(struct wayca_group *group, struct wayca_group *father)
{
	if (is_group_in_father(group, father))
		return 0;

	if (group->nr_cpus_per_topo >= father->nr_cpus_per_topo)
		return -1;

	father->nr_groups++;
	group_group_add_to_tail(group, father);

	group->father = father;

	if (wayca_group_rearrange_group(group)) {
		group_group_delete_group(group, father);
		father->nr_groups--;
		group->father = NULL;

		return -1;
	}

	return 0;
}

int wayca_group_delete_group(struct wayca_group *group, struct wayca_group *father)
{
	if (!is_group_in_father(group, father))
		return -1;

	if (CPU_COUNT(&father->used) == 0) {
		assert(father->roll_over_cnts > 0);

		father->roll_over_cnts--;
		CPU_OR(&father->used, &father->used, &father->total);
	}

	CPU_XOR(&father->used, &father->used, &father->total);

	group_group_delete_group(group, father);
	father->nr_groups--;
	group->father = NULL;

	return 0;
}

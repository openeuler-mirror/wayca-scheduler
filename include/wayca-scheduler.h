#ifndef WAYCA_SCHEDULER_H
#define WAYCA_SCHEDULER_H

#include <pthread.h>
#include <unistd.h>

int thread_bind_cpu(pid_t pid, int cpu);
int thread_bind_ccl(pid_t pid, int cpu);
int thread_bind_node(pid_t pid, int node);
int thread_bind_package(pid_t pid, int node);
int thread_unbind(pid_t pid);
int process_bind_cpu(pid_t pid, int cpu);
int process_bind_ccl(pid_t pid, int cpu);
int process_bind_node(pid_t pid, int node);
int process_bind_package(pid_t pid, int node);
int process_unbind(pid_t pid);
int process_bind_cpulist(pid_t pid, char *s);

int irq_bind_cpu(int irq, int cpu);

int mem_interleave_in_package(int node);
int mem_interleave_in_all(void);
int mem_bind_node(int node);
int mem_bind_package(int node);
int mem_unbind(void);
long mem_migrate_to_node(pid_t pid, int node);
long mem_migrate_to_package(pid_t pid, int node);

int cores_in_ccl(void);
int cores_in_node(void);
int cores_in_package(void);
int cores_in_total(void);
int nodes_in_package(void);
int nodes_in_total(void);

int wayca_managed_thread_create(int id, pthread_t *thread, const pthread_attr_t *attr,
				void *(*start_routine) (void *), void *arg);

int wayca_managed_threadpool_create(int id, int num, pthread_t *thread[],
				    const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);

#endif
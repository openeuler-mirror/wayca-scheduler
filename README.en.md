# wayca-deployer #
## Why? ##
Linux kernel scheduler is not smart enough to use the characteristics of programs to deploy tasks to correct CPUs.

- It isn't aware of the io_node programs are accessing.
- It has no idea if programs are sensitive to cache locality or to memory bandwidth.
- It is trying to spread tasks to more nodes and more CPUs to achieve load balance
- It has no idea of cluster(CCL)

wayca-deployer is a tool set which enables userspace deployment base on users' indication. Users will tell wayca-deployer how they are using CPU, memory, I/O and interrupt resources in a configuration file. Then wayca-deployer will try to achieve better performance with considering the locality of cache coherence, I/O and memory. On the other hand, wayca-deployer can also make memory interleave if users require high memory bandwidth.

## Software Architecture ##

### Client/Server ###
The deployment software consists of two parts - client and server.

```
+-----------------------------+                     +-------------------------+
| client                      |                     | server                  |
|    APP1-deployer.cfg        |                     |                         |
|                             |    socket           |   wayca-deployer.cfg    |
|       [PROG]                |                     |                         |
|       exec=example_thread1  +-------------------+ |     [SYS]               |
|       cpu_util=1            |                     |     occupied_cpus=1-2,4 |
|       io_node=0             |                     |                         |
|       cpu_bind=1            |                     +-------------+-----------+
|       irq_bind=978:2 979:3  |                                   |
|                             |                                   |
|                             |                                   |
+-----------------------------+                                   |
+------------------------------+                                  |
| client                       |                                  |
|   APP2-deployer.cfg          |                                  |
|                              |                                  |
|      [PROG]                  |                                  |
|      exec=example_thread4    |                                  |
|      cpu_util=4              |                                  |
|      io_node=0               |         socket                   |
|      cpu_bind=AUTO           |                                  |
|      mem_bandwith=LOW        +----------------------------------+
|                              |
|                              |
|                              |
+------------------------------+
```
### wayca-deployer ###
The client - wayca-deployer will deploy a program with the help of wayca-deployd based on a configuration file which describes the characteristics of the specific program. Instead of forking a child to run the program, wayca-deployer will directly execute a program by exec() family APIs and keep the process tree hierarchy unchanged in systemd and any similar init/subreaper system. On the other hand, wayca-deployer doesn't require any special privileged user accounts, alternatively, it just uses the user ID which the deployed program wants to use. For example, originally users might start example_prog using user ID John.
```
[Unit]
Description=example program

[Service]
ExecStart=/usr/bin/example_prog
User=John
```
In order to use wayca-deployer to schedule the example_progam more efficiently with the knowledge of the characteristics of this specific program, we only need to change the unit file to
```
[Unit]
Description=example program

[Service]
ExecStart=wayca-deployer -f example_prog.cfg /usr/bin/example_prog
User=John
```

### wayca-deployd ###
The server - wayca-deployd, on the other hand, will run as a daemon under privileged user accounts to collect all requests from wayca-deployer, and make smart deployment by by globally observing the usage of various resources such as memory, interrupts, CPUs. Since wayca-deployd is running under root-like users, it can do IRQ binding and other tasks which require privileged users. On the other hand, wayca-deployd has a global view of all programs deployed by wayca-deployer, which means that it can effectively coordinate in all these programs.

Similar to the wayca-deployer client, we can also place a configuration file on the server side to exclude some resources which have been occupied by other methods. For example, software like DPDK might have their own way to do resource binding rather than use wayca-deployer. The below example will tell wayca-deployd CPU 1,2,4 are totally not available to software deployed by wayca-deployer.

```
[SYS]
occupied_cpus=1-2,4
```

## Deployment strategy ##

As described above, users need to write a configuration file and pass it to wayca-deployer. In this configuration file, users can set their io_node, cpu_bind, mem_bandwidth and irq_bind etc. for those programs they want to deploy by wayca-deployer.

Next, let's see a couple of cases

### Coarse-grained binding ###

For the below configuration file example_three_thread.cfg
```
[PROG]
exec=example_three_thread
cpu_util=2
io_node=0
task_bind=12-13
irq_bind=978@2 979@3
```
We run the below command to deploy the program
```
wayca-deployer -f example_three_thread.cfg /user/bin/example/example_three_thread
```

wayca-deployer with the help of wayca-deployd will put all threads of this process on CPU 12 and 13. On the other hand,  `irq_bind=978:2 979:3` means IRQ 978 will be bound to CPU2 and IRQ 979 will be bound to CPU3.

With coarse-grained binding, the user cannot distinguish the specific threads in the process, it can only specify the overall strategy for the whole process.

### Fine-grained binding ###
For the below configuration file example_managed_thread.cfg
```
[PROG]
exec=example_managed_thread
cpu_util=5
io_node=0
task_bind=1@c1$1 2@n0$1 3-7@c5-7$3
mem_bandwidth=LOW
```
wayca-deployer will put
1. wayca-managed thread with ID 1 on CPU1
1. wayca-managed thread with ID 2 on NODE0
1. wayca-managed thread with ID 3-7 on CPU5-7.

The format is like THREADID@cCPUID$CPU_UTIL. For example, the below "task_bind" means
putting thread 1,3,4,5,9,10 on CPU 1,4,5,6,10 with estimated CPU utilization 5:
```
1,3-5,9-10@c1,4-6,10$5.
```
One more example, we do a 1:1 mapping between wayca-managed thread and CPU:
```
1@c10$1
2@c11$1
3@c12$1
4@c13$1
5@c14$1
```
Fine-grained binding only supports those threads created by wayca-managed APIs as below:
```
#include <wayca-scheduler.h>

void *thread_fun1(void *param)
{
        while (1) ;
}

void *thread_fun2(void *param)
{
        while (1) ;
}

void *threadpool_fun(void *param)
{
        while (1) ;
}

int main(void)
{
#define THREADS_IN_POOL 5
        int i;
        pthread_t tid1, tid2;
        pthread_t thd_pool[THREADS_IN_POOL];
        pthread_t *thd_pool_ptr[] =
            { thd_pool, thd_pool + 1, thd_pool + 2, thd_pool + 3, thd_pool + 4 };

        wayca_managed_thread_create(1, &tid1, NULL, thread_fun1, NULL);
        wayca_managed_thread_create(2, &tid2, NULL, thread_fun2, NULL);

        wayca_managed_threadpool_create(3, THREADS_IN_POOL, thd_pool_ptr, NULL,
                                        threadpool_fun, NULL);

        pthread_join(tid1, NULL);
        pthread_join(tid2, NULL);

        for (i = 0; i < THREADS_IN_POOL; i++)
                pthread_join(thd_pool[i], NULL);
}
```
For threads created by generic pthread_create(), we can only do coarse-grained binding or the below auto binding.

### Auto binding ###

If users'task_bind is not set to “AUTO”, wayca-deployer will bind tasks to the specified cpu list. Otherwise, wayca-deploy will make auto binding based on the below policy:
1. If memory bandwidth is LOW and cpu_util is less than a cluster, wayca-deployer will bind the process to a CCL which has enough idle COREs;
1. wayca-deploy will try to bind tasks to the NUMA node or the CPU package which I/O belongs to;
1. wayca-deploy will deploy tasks in a DIE, a PACKAGE or to all CORES if users require higher memory bandwidth than LOW to use more memory controllers;

For example, based on the below configuration file:
```
[PROG]
exec=example_ccl_auto
cpu_util=4
io_node=0
task_bind=AUTO
mem_bandwidth=LOW
```
wayca-deployd will try to put example_ccl_auto on one of CPU0-3, CPU4-7, CPU8-11, CPU12-15, CPU16-19, CPU20-23 on a machine which has 24 CPU in node0 and every 4 CPUs form a cluster. While making the decision of deployment, wayca-deployd will exclude the occupied_cpu in server configuration file and those clusters which have been used by coarse-grained binding and fine-grained binding to ensure example_ccl_auto is put on an idle CCL.

In all the above binding methods, wayca-deployer will set the right memory binding and interleave mode according to the memory bandwidth requirements implied by the user.
For instance, if users need relatively high memory bandwidth by using two memory controllers in two DIEs of one package parallel by setting mem_bandwidth to PACKAGE in the configuration file

```
[PROG]
exec=example_ccl_auto
cpu_util=4
io_node=0
task_bind=AUTO
mem_bandwidth=PACKAGE
```
wayca-deployer will set the program to perform memory interleave between two NUMA nodes.

## Other tiny tools ##
except wayca-deployer, the toolset also includes several tiny tools to benefit users' deployment. These tools are mainly used for debugging purposes.

### wayca-irqdump ###

Usually, it is tricky to figure out the irq number and irq name by `cat /proc/interrupts` on a Linux server as there are so many CPUs in this file. With wayca-irqdump, life becomes easier. You can run commands like:
```
 ./wayca-irqdump hns3
     irq       count     hns3
     199     27283644    65536001 Edge      hns3-0000:7d:00.0-TxRx-0
     329            0    65538049 Edge      hns3-0000:7d:00.1-TxRx-0
     394            0    65540097 Edge      hns3-0000:7d:00.2-TxRx-0
     524            0    65542145 Edge      hns3-0000:7d:00.3-TxRx-0
     589            0    99090433 Edge      hns3-0000:bd:00.0-TxRx-0
     ...
```
or
```
 ./wayca-irqdump smmu usb
     irq       count     smmu
      49            0    100352 Edge      arm-smmu-v3-evtq
      50            0    100353 Edge      arm-smmu-v3-gerror
      51            0    102400 Edge      arm-smmu-v3-evtq
      52            0    102401 Edge      arm-smmu-v3-gerror
      53            0    104448 Edge      arm-smmu-v3-evtq
      54            0    104449 Edge      arm-smmu-v3-gerror
      ...
     irq       count     usb
     978           91    63979520 Edge      ehci_hcd:usb1
     979            1    63963136 Edge      ohci_hcd:usb2
```
### wayca-irqdeploy ###

Rather than translating CPU ID to a HEX string and writing this string to /proc/irq/number/smp_affinity, wayca-irqdeploy implements this internally and offers users the below simple command:
```
./wayca-irqdeploy -i 978 -c 56
Changed irq 978's affinity to cpu 56
```
And you get:
```
cat /proc/irq/978/smp_affinity
00000000,01000000,00000000
```

### wayca-taskdeploy ###

This tool probably includes the functionalities of taskset, numactl and migratepages. Users can launch a new program by wayca-taskdeploy on specified CPUs and memory nodes. On the other hand, using wayca-taskdeploy, people can also migrate tasks and memory to specified CPUs and memory nodes for a running process.

It's also worth mentioning that wayca-taskdeploy can present the changes of `perf stat` for cache, bus, and cpu cycles and memory access after making the new deployment.

For example, the below command migrates process 88924 to CPU0-2 and moves the memory to node 1.

```
 ./wayca-taskdeploy -c 0-2 -m 1 -p 88924 -a
Changed task(s) 88924's affinity to cpu 0-2
Migrated task(s) 88924's pages to node 1
------[Performance changes after deployment]--------------
branch-misses                             65540   ->            66927 2.116265%
bus-cycles                          25999401542   ->      26007908739 0.032721%
cache-misses                          120486716   ->         97760648 -18.861887%
cycles                              25999479846   ->      26007910372 0.032426%
instructions                        43380141712   ->      35804566486 -17.463233%
inst per cycle                          1.6685   ->           1.3767 -17.489988%
stalled-cycles-backend              14811372777   ->      16967702897 14.558611%
stalled-cycles-frontend                58421169   ->         16239739 -72.202304%
bus_cycles                          25999650296   ->      26007875794 0.031637%
mem_access                          16701350201   ->      14295601684 -14.404515%
remote_access                             14338   ->            12571 -12.323895%
ll_cache                              238814068   ->        198829126 -16.743127%
ll_cache_miss                              6168   ->             6203 0.567445%
------[End]--------------
```

The below command starts a new program on cpu5-7 and memory 0:
```
./wayca-taskdeploy -e -c 5-7 -m 0 ./tests/thread4
starting app ./tests/thread4 on cpu5-7 memory0
```


## libwaycadeployer ##

libwaycadeployer doesn't depend other libraries except libc. Users can call the below APIs in libwaycadeployer:
```
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
int mem_migrate_to_node(pid_t pid, int node);
int mem_migrate_to_package(pid_t pid, int node);

int cores_in_ccl(void);
int cores_in_node(void);
int cores_in_package(void);
int cores_in_total(void);
int nodes_in_package(void);
int nodes_in_total(void);

int wayca_managed_thread_create(int id, pthread_t *thread, const pthread_attr_t *attr,
                void *(*start_routine) (void *), void *arg);

int wayca_managed_threadpool_create(int id, int num, pthread_t *thread[], const pthread_attr_t *attr,
                void *(*start_routine) (void *), void *arg);
```
The differences between libwaycadeployer and other libraries like libnuma are
- libwaycadeployer is much simpler, users can simply bind interrupts, threads, processes and memory etc.
- ccl, node, package, total are the cores of libwaycadeployer. This architecture-level abstraction is actually more useful than bitmask typical Linux APIs provide.
- wayca-managed thread. This permits users to do fine-grained control on each thread.

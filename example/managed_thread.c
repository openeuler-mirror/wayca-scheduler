#include <waycadeployer.h>

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

#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>

struct foo {
	int x;
	int y;
} f1;

void *thread_fun1(void *param)
{
	while (1) {
		int s = 0;
		for (int i = 0; i < 1000000000; ++i)
			s += f1.x;
	}
	return NULL;
}

void *thread_fun2(void *param)
{
	while (1) {
		for (int i = 0; i < 1000000000; ++i)
			++f1.y;
	}
	return NULL;
}

int main(void)
{
	pthread_t tid1, tid2, tid3, tid4;

	pthread_create(&tid1, NULL, thread_fun1, NULL);
	pthread_create(&tid2, NULL, thread_fun2, NULL);
	pthread_create(&tid3, NULL, thread_fun1, NULL);
	pthread_create(&tid4, NULL, thread_fun2, NULL);

	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);
	pthread_join(tid3, NULL);
	pthread_join(tid4, NULL);
}

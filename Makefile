#Todo: move to autoconf + automake
tools = wayca-deployer wayca-deployd wayca-irqdump wayca-irqdeploy wayca-taskdeploy
all: $(tools)
wayca-deployd: libwaycadeployer.so.1.0 deployd.c
	$(CC) $(CFLAGS) deployd.c -L. -lwaycadeployer -I./include -o $@
wayca-deployer: libwaycadeployer.so.1.0 deployer.c perf.c
	$(CC) $(CFLAGS) deployer.c perf.c -L. -lwaycadeployer -I./include -o $@
wayca-taskdeploy: libwaycadeployer.so.1.0 taskdeploy.c
	$(CC) $(CFLAGS) taskdeploy.c perf.c -L. -lwaycadeployer -I./include -o $@
wayca-irqdeploy: libwaycadeployer.so.1.0 irqdeploy.c
	$(CC) $(CFLAGS) irqdeploy.c -L. -lwaycadeployer -I./include -o $@
wayca-irqdump: libwaycadeployer.so.1.0 irqdump.c
	$(CC) $(CFLAGS) irqdump.c -L. -lwaycadeployer -I./include -o $@
libwaycadeployer.so.1.0: lib/threads.o lib/managed_threads.o lib/irq.o lib/mem.o lib/topo.o lib/group.o
	$(CC) -fPIC -pthread -shared -Wl,-soname,libwaycadeployer.so.1 -o $@ $^
	-ln -s libwaycadeployer.so.1.0 libwaycadeployer.so
	-ln -s libwaycadeployer.so.1.0 libwaycadeployer.so.1

CFLAGS +=-g -Wall -fPIC -DWAYCA_DEPLOY_VERSION=\"0.1\"
SRCS = $(wildcard *.c)
OBJS =$(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)
$(DEPS) : %.d : %.c
	gcc -MM $< > $@
SRCS = $(wildcard *.c)
%.o : %.c
	$(CC) -c $(CFLAGS) -I./include -o $@ $^

install:
	install *.so* /usr/lib/
	install $(tools) /usr/bin
	install include/wayca-scheduler.h /usr/include
clean:
	-rm *.o lib/*.o
	-rm *.so*
	-rm $(tools)

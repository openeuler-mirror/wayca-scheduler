#Todo: move to autoconf + automake
tools = wayca-deployer wayca-deployd wayca-irqdump wayca-irqdeploy wayca-taskdeploy
tests = wayca_sc_group wayca_thread wayca_topo wayca_bitmap

all: $(tools) $(tests)
wayca-deployd: libwaycadeployer.so.1.0 deployd.c
	$(CC) $(CFLAGS) deployd.c -L. -lwaycadeployer -I./include -o $@
wayca-deployer: libwaycadeployer.so.1.0 deployer.c perf.c
	$(CC) $(CFLAGS) deployer.c perf.c -L. -lwaycadeployer -I./include -o $@
wayca-taskdeploy: libwaycadeployer.so.1.0 taskdeploy.c perf.c
	$(CC) $(CFLAGS) taskdeploy.c perf.c -L. -lwaycadeployer -I./include -o $@
wayca-irqdeploy: libwaycadeployer.so.1.0 irqdeploy.c
	$(CC) $(CFLAGS) irqdeploy.c -L. -lwaycadeployer -I./include -o $@
wayca-irqdump: libwaycadeployer.so.1.0 irqdump.c
	$(CC) $(CFLAGS) irqdump.c -L. -lwaycadeployer -I./include -o $@
libwaycadeployer.so.1.0: lib/threads.o lib/managed_threads.o lib/irq.o lib/mem.o lib/topo.o lib/group.o lib/log.o
	$(CC) -fPIC -pthread -shared -Wl,-soname,libwaycadeployer.so.1 -o $@ $^
	-ln -s libwaycadeployer.so.1.0 libwaycadeployer.so
	-ln -s libwaycadeployer.so.1.0 libwaycadeployer.so.1
# test stubs
wayca_sc_group: libwaycadeployer.so.1.0 test/wayca_sc_group.c
	$(CC) $(CFLAGS) test/wayca_sc_group.c -L. -lwaycadeployer -I./include -o test/$@
wayca_thread: libwaycadeployer.so.1.0 test/wayca_thread.c
	$(CC) $(CFLAGS) test/wayca_thread.c -L. -lwaycadeployer -I./include -o test/$@
wayca_topo: libwaycadeployer.so.1.0 test/wayca_topo.c
	$(CC) $(CFLAGS) test/wayca_topo.c -L. -lwaycadeployer -I./include -o test/$@
wayca_bitmap: test/wayca_bitmap.c
	$(CC) $(CFLAGS) test/wayca_bitmap.c -I./lib -o test/$@

CFLAGS += -Wall -fPIC -DWAYCA_DEPLOY_VERSION=\"0.1\"
ifeq ($(DEBUG), 1)
  CFLAGS += -g -DWAYCA_SC_DEBUG
endif

SRCS = $(wildcard *.c)
OBJS =$(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)
CC = $(CROSS_COMPILE)gcc
INSTALL_PREFIX = /usr
$(DEPS) : %.d : %.c
	$(CROSS_COMPILE)gcc -MM $< > $@
SRCS = $(wildcard *.c)
%.o : %.c
	$(CC) -c $(CFLAGS) -I./include -o $@ $^

install:
	install -d -m 755 $(INSTALL_PREFIX)/lib
	install -d -m 755 $(INSTALL_PREFIX)/bin
	install -d -m 755 $(INSTALL_PREFIX)/include
	install *.so* $(INSTALL_PREFIX)/lib/
	install $(tools) $(INSTALL_PREFIX)/bin
	install include/wayca-scheduler.h $(INSTALL_PREFIX)/include
clean:
	-rm -f *.o lib/*.o
	-rm -f *.so*
	-rm -f $(tools)
	-cd test && rm -f $(tests)

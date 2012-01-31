CFLAGS = -g -Wall -O3 -D_GNU_SOURCE -DNDEBUG -std=gnu99
LDLIBS+=-lm

.PHONY: all clean run

TARGETS=pipe_thr tcp_thr tcp_nodelay_thr unix_thr mempipe_thr mempipe_spin_thr
TARGETS+=vmsplice_pipe_thr vmsplice_hugepages_pipe_thr vmsplice_hugepages_coop_pipe_thr vmsplice_coop_pipe_thr
TARGETS+=shmem_pipe_thr futex_lat
TARGETS+=pipe_lat unix_lat tcp_lat tcp_nodelay_lat mempipe_lat
TARGETS+=summarise_tsc_counters

all: $(TARGETS)

%_lat: atomicio.o test.o xutil.o stats.o %_lat.o
	$(CC) $(CFLAGS) -o $@ $^ -lrt -lnuma -lm

%_thr: atomicio.o test.o xutil.o stats.o %_thr.o
	$(CC) $(CFLAGS) -o $@ $^ -lrt -lnuma -lm

tcp_nodelay_thr.o: tcp_thr.c
	$(CC) $(CFLAGS) $^ -c -DUSE_NODELAY -o $@

tcp_nodelay_lat.o: tcp_lat.c
	$(CC) $(CFLAGS) $^ -c -DUSE_NODELAY -o $@

mempipe_spin_thr.o: mempipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DNO_FUTEX -o $@

vmsplice_hugepages_pipe_thr.o: vmsplice_pipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DUSE_HUGE_PAGES -o $@

vmsplice_hugepages_coop_pipe_thr.o: vmsplice_pipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DUSE_HUGE_PAGES -DVMSPLICE_COOP -o $@	

vmsplice_coop_pipe_thr.o: vmsplice_pipe_thr.c
	$(CC) $(CFLAGS) $^ -c -DVMSPLICE_COOP -o $@

summarise_tsc_counters: summarise_tsc_counters.o stats.o

clean:
	rm -f *~ core *.o $(TARGETS)


all_target := posix
uname := $(shell uname -s)
ifeq ($(uname),Linux)
all_target := linux
endif
ifeq ($(uname),OpenBSD)
all_target := openbsd
endif
ifeq ($(uname),Darwin)
all_target := darwin
endif

CFLAGS = -g -Wall -O3 -D_GNU_SOURCE -DNDEBUG -std=gnu99

.PHONY: all clean run

CFLAGS_Linux = -DUSE_INLINE_ASM -DLinux
CFLAGS_OpenBSD = -DUSE_INLINE_ASM
CFLAGS += -g -Wall -O3 -D_GNU_SOURCE -DNDEBUG -std=gnu99 $(CFLAGS_$(uname))

LDFLAGS_Linux := -lrt -lnuma
LDFLAGS += -lm $(LDFLAGS_$(uname))

TARGETS_POSIX := pipe_thr tcp_thr tcp_nodelay_thr unix_thr mempipe_spin_thr
TARGETS_Linux += mempipe_thr vmsplice_pipe_thr vmsplice_hugepages_pipe_thr vmsplice_hugepages_coop_pipe_thr vmsplice_coop_pipe_thr

TARGETS_POSIX += pipe_lat unix_lat tcp_lat tcp_nodelay_lat mempipe_lat
TARGETS_Linux += shmem_pipe_thr futex_lat

TARGETS_POSIX += summarise_tsc_counters

TARGETS_OpenBSD := 
TARGETS_Darwin :=

TARGETS := $(TARGETS_POSIX) $(TARGETS_$(uname))

x-%:
	echo $($*)

all: $(TARGETS)
	@ :

%_lat: atomicio.o test.o xutil.o %_lat.o stats.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%_thr: atomicio.o test.o xutil.o %_thr.o stats.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

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


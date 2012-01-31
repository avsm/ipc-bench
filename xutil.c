/* 
    Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>
    Copyright (c) 2011 Steven Smith <sos22@cam.ac.uk>

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use,
    copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following
    conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
*/
#include <sys/mman.h>
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <numa.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <inttypes.h>
#include "atomicio.h"
#include "xutil.h"
#include "test.h"

void *
xmalloc(size_t size)
{
  void *buf;
  buf = malloc(size);
  if (buf == NULL)
    err(1, "xmalloc");
  return buf;
}

void
xread(int fd, void *buf, size_t count)
{
  ssize_t r;
  r = atomicio(read, fd, buf, count);
  if (r != count)
    err(1, "xread");
}

void
xwrite(int fd, const void *buf, size_t count)
{
  ssize_t r;
  r = atomicio(vwrite, fd, (void *)buf, count);
  if (r != count)
    err(1, "xwrite");
}

static double
get_tsc_freq(void)
{
  double estimates[5];
  int nr_estimates;
  struct timeval start;
  struct timeval now;
  unsigned long start_tsc;
  unsigned long end_tsc;
  double low_estimate, high_estimate;
  double total;
  int i;

  while (1) {
    for (nr_estimates = 0; nr_estimates < 5; nr_estimates++) {
      gettimeofday(&start, NULL);
      start_tsc = rdtsc();
      do {
	gettimeofday(&now, NULL);
	now.tv_sec -= start.tv_sec;
	now.tv_usec -= start.tv_usec;
	if (now.tv_usec < 0) {
	  now.tv_sec--;
	  now.tv_usec += 1e6;
	}
      } while (now.tv_usec < 1e4);
      end_tsc = rdtsc();
      estimates[nr_estimates] = (end_tsc - start_tsc) / (now.tv_usec * 1e-6);
    }

    /* Check for anomalies */
    low_estimate = estimates[0];
    high_estimate = estimates[0];
    for (i = 1; i < nr_estimates; i++) {
      if (estimates[i] < low_estimate)
	low_estimate = estimates[i];
      if (estimates[i] > high_estimate)
	high_estimate = estimates[i];
    }
    /* Accept if range of estimates is less than 1% */
    if (high_estimate < low_estimate * 1.01)
      break;
    /* Otherwise try again */
  }

  /* Use an arithmetic mean.  Should arguably be using harmonic mean
     here, but since the thinks we're averaging vary by less than 1%
     it shouldn't make much difference. */
  total = 0;
  for (i = 0; i < nr_estimates; i++)
    total += estimates[i];
  return total / nr_estimates;
}

static FILE *
open_logfile(test_data *td, const char *file)
{
  char *path;
  FILE *res;

  assert(td->output_dir != NULL);

  if (asprintf(&path,
	       "%s/%02d-%s-%s.log",
	       td->output_dir,
	       td->num,
	       td->name,
	       file) < 0)
    err(1, "asprintf()");
  res = fopen(path, "a");
  if (!res)
    err(1, "fopen(%s)", path);
  free(path);
  return res;
}

void
dump_tsc_counters(test_data *td, unsigned long *counts, int nr_samples)
{
  FILE *f = open_logfile(td, "tsc");
  double clock_freq = get_tsc_freq();
  int i;
  double *times = (double *)counts;
  times = calloc(sizeof(double), nr_samples);
  for (i = 0; i < nr_samples; i++)
    times[i] = counts[i] / clock_freq;
  free(counts);
  summarise_samples(f, times, nr_samples);
  fclose(f);

#ifdef DUMP_RAW_TSCS
  f = open_logfile(td, "raw_tsc");
  for (i = 0; i < nr_samples; i++)
    fprintf(f, "%e\n", times[i]);
  fclose(f);
#endif

  free(times);
}

static void
help(char *argv[])
{
  fprintf(stderr, "Usage:\n%s [-h] [-a <cpuid>] [-b <cpuid>] [-p <num] [-t] [-s <bytes>] [-c <num>] [-o <directory>] [-n <node>]\n", argv[0]);
  fprintf(stderr, "-h: show this help message\n");
  fprintf(stderr, "-a: CPU id that the first process should have affinity with\n");
  fprintf(stderr, "-b: CPU id that the second process should have affinity with\n");
  fprintf(stderr, "-p: number of parallel tests to run\n");
  fprintf(stderr, "-t: use high-res TSC to get more accurate results\n");
  fprintf(stderr, "-s: Size of each packet\n");
  fprintf(stderr, "-c: Number of iterations\n");
  fprintf(stderr, "-o: Where to put the various output files\n");
  fprintf(stderr, "-n: NUMA node for shared arena, if any\n");
  exit(1);
}

void
parse_args(int argc, char *argv[], bool *per_iter_timings, int *size, size_t *count, int *first_cpu, int *second_cpu,
	   int *parallel, char **output_dir, int *write_in_place, int *read_in_place, int *produce_method, int *do_verify,
	   int *numa_node)
{
  int opt;
  *per_iter_timings = false;
  *first_cpu = 0;
  *second_cpu = 0;
  *parallel = 1;
  *size = 1024;
  *count = 100;
  *output_dir = "results";
  *numa_node = -1;
  *produce_method = 0;
  *read_in_place = 0;
  *write_in_place = 0;
  *do_verify = 0;
  while((opt = getopt(argc, argv, "h?tp:a:b:s:c:o:wrvm:n:")) != -1) {
    switch(opt) {
     case 't':
      *per_iter_timings = true;
      break;
     case 'p':
      *parallel = atoi(optarg);
      break;
     case 'a':
      *first_cpu = atoi(optarg);
      break;
     case 'b':
      *second_cpu = atoi(optarg);
      break;
     case 's':
      *size = atoi(optarg);
      break;
     case 'c':
      *count = atoi(optarg);
      break;
     case 'o':
      *output_dir = optarg;
      break;
     case 'm':
      *produce_method = atoi(optarg);
      break;
     case 'r':
      *read_in_place = 1;
      break;
     case 'w':
      *write_in_place = 1;
      break;
    case 'v':
      *do_verify = 1;
      break;
    case 'n':
      *numa_node = atoi(optarg);
      break;
     case '?':
     case 'h':
      help(argv);
    default:
      fprintf(stderr, "unknown command-line option '%c'", opt);
      help(argv);
    }
  }

  fprintf(stderr, "size %d count %" PRId64 " first_cpu %d second_cpu %d parallel %d tsc %d produce-method %d %s %s numa_node %d output_dir %s\n",
	  *size, *count, *first_cpu, *second_cpu, *parallel, *per_iter_timings, *produce_method, *read_in_place ? "read-in-place" : "copy-read", *write_in_place ? "write-in-place" : "copy-write",
	  *numa_node,
	  *output_dir);
}

void
setaffinity(int cpunum)
{
  cpu_set_t *mask;
  size_t size;
  int i;
  int nrcpus = 160;
  pid_t pid;
  mask = CPU_ALLOC(nrcpus);
  size = CPU_ALLOC_SIZE(nrcpus);
  CPU_ZERO_S(size, mask);
  CPU_SET_S(cpunum, size, mask);
  pid = getpid();
  i = sched_setaffinity(pid, size, mask);
  if (i == -1)
    err(1, "sched_setaffinity");
  CPU_FREE(mask);
}

void *
establish_shm_segment(int nr_pages, int numa_node)
{
  int fd;
  void *addr;
  struct bitmask *alloc_nodes;
  struct bitmask *old_mask;

  fd = shm_open("/memflag_lat", O_RDWR|O_CREAT|O_EXCL, 0600);
  if (fd < 0)
    err(1, "shm_open(\"/memflag_lat\")");
  shm_unlink("/memflag_lat");
  if (ftruncate(fd, PAGE_SIZE * nr_pages) < 0)
    err(1, "ftruncate() shared memory segment");
  addr = mmap(NULL, PAGE_SIZE * nr_pages, PROT_READ|PROT_WRITE, MAP_SHARED,
	      fd, 0);
  if (addr == MAP_FAILED)
    err(1, "mapping shared memory segment");

  if(numa_node != -1)
    numa_tonode_memory(addr, PAGE_SIZE * nr_pages, numa_node);

  close(fd);

  return addr;
}

void
logmsg(test_data *td, const char *file, const char *fmt, ...)
{
  FILE *f = open_logfile(td, file);
  va_list args;
  char *res;

  va_start(args, fmt);
  if (vasprintf(&res, fmt, args) < 0)
    err(1, "vasprintf(%s)", fmt);
  va_end(args);

  if (fputs(res, f) == EOF)
    err(1, "fputs(%s) to logfile:%s", res, file);

  if (fclose(f) == EOF)
    err(1, "fclose(logfile:%s)", file);
}

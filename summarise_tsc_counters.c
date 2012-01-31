#include <assert.h>
#include <err.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "xutil.h"

static void
read_input(double **_res, int *nr_samples)
{
  int n = 0;
  int n_alloced = 0;
  double *res = NULL;
  double i;
  int r;

  while (!feof(stdin)) {
    if (n_alloced == n) {
      n_alloced += 1024;
      res = realloc(res, sizeof(res[0]) * n_alloced);
      if (!res)
	err(1, "realloc");
    }
    r = scanf("%le\n", &i);
    if (r < 0)
      err(1, "scanf");
    if (r == 0 && feof(stdin))
      break;
    if (r != 1)
      errx(1, "scanf returned unexpected value %d", r);
    res[n] = i;
    n++;
  }
  *_res = res;
  *nr_samples = n;
}

int
main()
{
  double *times;
  int nr_samples;

  read_input(&times, &nr_samples);

  summarise_samples(stdout, times, nr_samples);

  return 0;
}

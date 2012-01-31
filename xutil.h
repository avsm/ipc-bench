/* 
    Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>

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

#include <stdbool.h>
#include <stdio.h>

void * xmalloc(size_t);
void xread(int, void *, size_t);
void xwrite(int, const void *, size_t);

void setaffinity(int);
void parse_args(int argc, char *argv[], bool *per_iter_timings, int *size, size_t *count,
		int *first_cpu, int *second_cpu, int *parallel, char **output_dir, int *wip, int *rip, int *prod, int *do_verify,
		int *numa_node);
void *establish_shm_segment(int nr_pages, int numa_node);

/* Doesn't really belong here, but doesn't really belong anywhere. */
void summarise_samples(FILE *f, double *data, int nr_samples);

#define PAGE_SIZE 4096

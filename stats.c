#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "xutil.h"

struct summary_stats {
  const double *data;
  int nr_items;

  double mean;
  double sample_sd;
  double sample_skew;
  double sample_kurtosis;
};

static int
compare_double(const void *_a, const void *_b)
{
  const double *a = _a;
  const double *b = _b;
  if (*a < *b)
    return -1;
  else if (*a == *b)
    return 0;
  else
    return 1;
}

static void
calc_summary_stats(const double *data, int nr_items, struct summary_stats *out)
{
  /* On-line calculation of mean, variance, skew and kurtosis
     lifted straight from wikipedia. */
  double mean = 0;
  double m2 = 0;
  double m3 = 0;
  double m4 = 0;
  double delta;
  double delta_n;
  double variance;
  double sd;
  double skew;
  double kurtosis;
  double n;
  int i;

  for (i = 0; i < nr_items; i++) {
    n = i + 1;
    delta = data[i] - mean;
    delta_n = delta / n;
    mean = (mean * i) / n + data[i]/n;
    m4 = m4 + delta_n * delta_n * delta_n * delta * (n - 1) * (n * n - 3 * n + 3) + 6 * delta_n * delta_n * m2 - 4 * delta_n * m3;
    m3 = m3 + delta_n * delta_n * delta * (n - 1) * (n - 2) - 3 * delta_n * m2;
    m2 = m2 + delta_n * delta * (n - 1);
  }

  variance = m2 / nr_items;
  sd = sqrt(variance);
  skew = m3/(nr_items * sd * sd * sd);
  kurtosis = nr_items * m4 / (m2*m2) - 3;

  out->mean = mean;
  out->sample_sd = sd;
  out->sample_skew = skew;
  out->sample_kurtosis = kurtosis;

  out->data = data;
  out->nr_items = nr_items;
}

static double
point_to_percentile(const struct summary_stats *ss, double point)
{
  double y1, y2, num, denum;
  int low, high;
  int probe;

  if (point < ss->data[0])
    return 0;
  else if (point > ss->data[ss->nr_items-1])
    return 100;
  low = 0;
  high = ss->nr_items;
  while (low + 1 < high) {
    /* Invariant: everything in slots before @low is less than @point,
       everything in slots at or after @high is greater than
       @point. */
    probe = (high + low) / 2;
    assert(probe != low);
    if (point > ss->data[probe]) {
      low = probe + 1;
    } else if (point < ss->data[probe]) {
      high = probe;
    } else {
      /* The probe is now in the range of data which is equal to
	 point. */
      goto probe_is_point;
    }
  }
  if (high == low + 1) {
    if (point < ss->data[low]) {
      assert(low != 0);
      assert(point > ss->data[low-1]);
      low--;
      high--;
    }
    if (ss->data[low] == point) {
      probe = low;
      goto probe_is_point;
    } else if (ss->data[high] == point) {
      probe = high;
      goto probe_is_point;
    } else {
      goto linear_interpolate;
    }
  } else {
    assert(high == low);
    if (low == 0) {
      return 0;
    } else {
      low = high - 1;
      goto linear_interpolate;
    }
  }

 probe_is_point:
  low = probe;
  while (low >= 0 && ss->data[low] == point)
    low--;
  high = probe;
  while (high < ss->nr_items && ss->data[high] == point)
    high++;
  return (high + low) * 50.0 / ss->nr_items;

 linear_interpolate:
  y1 = ss->data[low];
  y2 = ss->data[high];
  num = (point + y2 * low - high * y1) * 100.0 / ss->nr_items;
  denum = y2 - y1;
  if (fabs(denum / num) < 0.01) {
    /* The two points we're trying to interpolate between are so close
       together that we risk numerical error, so we can't use the
       normal formula.  Fortunately, if they're that close together
       then it doesn't really matter, and we can use a simple
       average. */
    return (low + high) * 50.0 / ss->nr_items;
  } else {
    return num / denum;
  }
}

static void
print_summary_stats(FILE *f, const struct summary_stats *ss)
{
  double sd_percentiles[7];
  int i;

  fprintf(f, "\tMean %e, sample sd %e, sample skew %e, sample kurtosis %e\n",
	 ss->mean, ss->sample_sd, ss->sample_skew, ss->sample_kurtosis);
  fprintf(f, "\tQuintiles: %e, %e, %e, %e, %e, %e\n",
	 ss->data[0],
	 ss->data[ss->nr_items / 5],
	 ss->data[ss->nr_items * 2 / 5],
	 ss->data[ss->nr_items * 3 / 5],
	 ss->data[ss->nr_items * 4 / 5],
	 ss->data[ss->nr_items - 1]);
  fprintf(f, "\t5%% %e, median %e, 95%% %e\n",
	 ss->data[ss->nr_items / 20],
	 ss->data[ss->nr_items / 2],
	 ss->data[ss->nr_items * 19 / 20]);

  /* Also look at how deltas from the mean, in multiples of the SD,
     map onto percentiles, to get more hints about non-normality. */
  for (i = 0; i < 7; i++) {
    double point = ss->mean + ss->sample_sd * (i - 3);
    sd_percentiles[i] = point_to_percentile(ss, point);
  }
  fprintf(f, "\tSD percentiles: -3 -> %f%%, -2 -> %f%%, -1 -> %f%%, 0 -> %f%%, 1 -> %f%%, 2 -> %f%%, 3 -> %f%%\n",
	 sd_percentiles[0],
	 sd_percentiles[1],
	 sd_percentiles[2],
	 sd_percentiles[3],
	 sd_percentiles[4],
	 sd_percentiles[5],
	 sd_percentiles[6]);
}

static void
linear_regression(const double *data, int nr_samples, double *alpha, double *beta)
{
  double x_bar;
  double x_bar2;
  double x_y_bar;
  double y_bar;
  int i;

  x_y_bar = 0;
  y_bar = 0;

  for (i = 0; i < nr_samples; i++) {
    x_y_bar += data[i] * (i + 1);
    y_bar += data[i];
  }
  x_y_bar /= nr_samples;
  y_bar /= nr_samples;

  x_bar = nr_samples / 2.0 + 1;
  x_bar2 = (nr_samples + 2.0) * (2.0 * nr_samples + 1) / 6.0;

  *beta = (x_y_bar - x_bar * y_bar) / (x_bar2 - x_bar * x_bar);
  *alpha = y_bar - *beta * x_bar;

  /* Norm so that xs run from 0 to 1, rather than from 0 to
     nr_samples, because that's a bit easier to think about. */
  *beta *= nr_samples;
}

void
summarise_samples(FILE *f, double *data, int nr_samples)
{
  struct summary_stats whole_dist_stats;
  struct summary_stats low_outliers;
  struct summary_stats high_outliers;
  struct summary_stats excl_outliers;
  int i;
  int low_thresh, high_thresh;
  int discard;
  double alpha, beta;

  /* Discard the first few samples, so as to avoid startup
     transients. */
  discard = nr_samples / 20;
  data += discard;
  nr_samples -= discard;

  linear_regression(data, nr_samples, &alpha, &beta);
  fprintf(f, "Linear regression: y = %e*x + %e\n", beta, alpha);

  if (nr_samples >= 30) {
    fprintf(f, "By tenths of total run:\n");
    for (i = 0; i < 10; i++) {
      struct summary_stats stats;
      int start = (nr_samples * i) / 10;
      int end = (nr_samples * (i+1)) / 10;
      qsort(data + start, end - start, sizeof(data[0]), compare_double);
      calc_summary_stats(data + start, end - start, &stats);
      fprintf(f, "Slice %d/10:\n", i);
      print_summary_stats(f, &stats);
    }
  }

  qsort(data, nr_samples, sizeof(data[0]), compare_double);

  calc_summary_stats(data, nr_samples, &whole_dist_stats);

  fprintf(f, "Distribution of all values:\n");
  print_summary_stats(f, &whole_dist_stats);

#define OUTLIER 10
  low_thresh = nr_samples / OUTLIER;
  high_thresh = nr_samples - nr_samples / OUTLIER;
#undef OUTLIER
  if (low_thresh >= high_thresh ||
      low_thresh == 0 ||
      high_thresh == nr_samples)
    return;
  calc_summary_stats(data, low_thresh, &low_outliers);
  calc_summary_stats(data + low_thresh, high_thresh - low_thresh, &excl_outliers);
  calc_summary_stats(data + high_thresh, nr_samples - high_thresh, &high_outliers);

  fprintf(f, "Low outliers:\n");
  print_summary_stats(f, &low_outliers);

  fprintf(f, "Bulk distribution:\n");
  print_summary_stats(f, &excl_outliers);

  fprintf(f, "High outliers:\n");
  print_summary_stats(f, &high_outliers);
}


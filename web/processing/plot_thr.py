import sys, os

import matplotlib as mpl
mpl.use("Agg")
import numpy as np
import matplotlib.pyplot as plt
import pylab as pyl

def get_data(filename, is_series, plot_chunksizes):
  dst_core_colid = 1
  chunksize_colid = 3
  safe_colid = 6

#  print "%s %d" % (filename, is_series)

  if is_series:
    data = np.loadtxt(filename,
                      usecols=(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21))
    throughput_colid = 19
    stddev_colid = 20
  else:
    data = np.loadtxt(filename, usecols=(1,2,3,4,5,6,7,8,9,10))
    throughput_colid = 9

  retdata = {}
  maxval = 0
  for i in range(len(data)):
#    print "%d: %f (safe: %d, chunksize: %d)" % (data[i][dst_core_colid],
#                                                data[i][throughput_colid],
#                                                data[i][safe_colid],
#                                                data[i][chunksize_colid])
    dst_core = data[i][dst_core_colid]
    chunksize = data[i][chunksize_colid]
    # Skip chunk sizes that we have data for, but do not want to plot
    if chunksize not in plot_chunksizes:
      continue
    safe = data[i][safe_colid]
    throughput = data[i][throughput_colid]
    maxval = max(throughput, maxval)
    if is_series:
      stddev = data[i][stddev_colid]
    else:
      stddev = 0
    if not dst_core in retdata:
      retdata[dst_core] = {0: {}, 1: {}}
    retdata[dst_core][safe][chunksize] = (throughput, stddev)
  return (retdata, maxval)

def get_series(data, dst_core, safe=1):
  values = [v[0] for c, v in data[dst_core][safe].items()]
#  print str(dst_core) + ": " + str(values)
  return values

def get_stddev_series(data, dst_core, safe=1):
  values = [v[1] for c, v in data[dst_core][safe].items()]
#  print str(dst_core) + ": " + str(values)
  return values

def autolabel(rects):
  # attach some text labels
  for rect in rects:
    height = rect.get_height()
    ax.text(rect.get_x()+rect.get_width()/1.98, height+400, '%d'%int(height),
            ha='center', va='bottom', rotation='vertical', size='x-small')

# ------------------------------------------------------------------
# Modify the below variables for different experimental setups
# - in particular, for different machines, modify cores
#   (tigger's config is [0, 1, 6, 12, 18])

#cores = [0, 1, 6, 18]  # the core IDs benchmarked (first ID of pair is always 0)
cores = [int(c) for c in sys.argv[2].split(",")]
chunksizes = [64, 4096, 65536]
n_bars = 11   # number of bars (tests) per group
labels = ['(a)', '(b)', '(c)', '(d)', '(e)']

n_groups = len(chunksizes)  # number of chunk sizes

# ---------------------------
# Handle command line args

if len(sys.argv) < 3:
  print "usage: python plot_thr.py <results directory> <target_cpus> <is_series: (0|1)> [filenames ...]"
  sys.exit(0)

res_dir = sys.argv[1]
output_dir = res_dir + "/../graphs"

is_series = int(sys.argv[3])

# ---------------------------
# Get result filenames

if len(sys.argv) > 4:
  mempipe_spin_filename = sys.argv[4]
else:
  mempipe_spin_filename = res_dir + '/01-mempipe_spin_thr-headline.log'

if len(sys.argv) > 5:
  mempipe_futex_filename = sys.argv[5]
else:
  mempipe_futex_filename = res_dir + '/01-mempipe_thr-headline.log'

if len(sys.argv) > 6:
  shmem_pipe_filename = sys.argv[6]
else:
  shmem_pipe_filename = res_dir + '/01-shmem_pipe_thr-headline.log'

if len(sys.argv) > 7:
  vmsplice_filename = sys.argv[7]
else:
  vmsplice_filename = res_dir + '/01-vmsplice_coop_pipe_thr-headline.log'

if len(sys.argv) > 8:
  pipe_filename = sys.argv[8]
else:
  pipe_filename = res_dir + '/01-pipe_thr-headline.log'

if len(sys.argv) > 9:
  unix_filename = sys.argv[9]
else:
  unix_filename = res_dir + '/01-unix_thr-headline.log'

if len(sys.argv) > 10:
  tcp_nd_filename = sys.argv[10]
else:
  tcp_nd_filename = res_dir + '/01-tcp_nodelay_thr-headline.log'

if len(sys.argv) > 11:
  tcp_filename = sys.argv[11]
else:
  tcp_filename = res_dir + '/01-tcp_thr-headline.log'

# --------------------------

# Overall maximum value (for upper y-axis limit); note that get_data may modify
# this variable
all_max = 10000

mempipe_spin_data = get_data(mempipe_spin_filename, is_series, chunksizes)
all_max = max(mempipe_spin_data[1], all_max)
mempipe_futex_data = get_data(mempipe_futex_filename, is_series, chunksizes)
all_max = max(mempipe_futex_data[1], all_max)
shmpipe_data = get_data(shmem_pipe_filename, is_series, chunksizes)
all_max = max(shmpipe_data[1], all_max)
vmsplice_data = get_data(vmsplice_filename, is_series, chunksizes)
all_max = max(vmsplice_data[1], all_max)
pipe_data = get_data(pipe_filename, is_series, chunksizes)
all_max = max(pipe_data[1], all_max)
unix_data = get_data(unix_filename, is_series, chunksizes)
all_max = max(unix_data[1], all_max)
tcp_nd_data = get_data(tcp_nd_filename, is_series, chunksizes)
all_max = max(tcp_nd_data[1], all_max)
tcp_data = get_data(tcp_filename, is_series, chunksizes)
all_max = max(tcp_data[1], all_max)

fig_idx = 1
fig = plt.figure(figsize=(6,4))
pyl.rc('font', size='8.0')

for dst_core in cores:
  # get data series
  if dst_core != 0:
    mempipe_spin_unsafe_series = get_series(mempipe_spin_data[0], dst_core, safe=0)
    mempipe_spin_safe_series = get_series(mempipe_spin_data[0], dst_core, safe=1)
  else:
    # if we have dst_core set to 0 (i.e. we're communicating with ourselves), we skip
    # the spin test, so we set the values to zero here (they are not in the result files)
    mempipe_spin_unsafe_series = [0] * n_groups
    mempipe_spin_safe_series = [0] * n_groups
  mempipe_futex_unsafe_series = get_series(mempipe_futex_data[0], dst_core, safe=0)
  mempipe_futex_safe_series = get_series(mempipe_futex_data[0], dst_core, safe=1)
  shmpipe_unsafe_series = get_series(shmpipe_data[0], dst_core, safe=1)
  shmpipe_safe_series = get_series(shmpipe_data[0], dst_core, safe=0)
  vmsplice_series = get_series(vmsplice_data[0], dst_core)
  pipe_series = get_series(pipe_data[0], dst_core)
  unix_series = get_series(unix_data[0], dst_core)
  tcp_nd_series = get_series(tcp_nd_data[0], dst_core)
  tcp_series = get_series(tcp_data[0], dst_core)

  if is_series:
    if dst_core != 0:
      mempipe_spin_unsafe_stddev_series = get_stddev_series(mempipe_spin_data,
                                                            dst_core, safe=0)
      mempipe_spin_safe_stddev_series = get_stddev_series(mempipe_spin_data,
                                                          dst_core, safe=1)
    else:
      mempipe_spin_unsafe_stddev_series = [0] * n_groups
      mempipe_spin_safe_stddev_series = [0] * n_groups
    mempipe_futex_unsafe_stddev_series = get_stddev_series(mempipe_futex_data,
                                                           dst_core, safe=0)
    mempipe_futex_safe_stddev_series = get_stddev_series(mempipe_futex_data,
                                                         dst_core, safe=1)
    shmpipe_unsafe_stddev_series = get_stddev_series(shmpipe_data, dst_core,
                                                     safe=1)
    shmpipe_safe_stddev_series = get_stddev_series(shmpipe_data, dst_core,
                                                   safe=0)
    vmsplice_stddev_series = get_stddev_series(vmsplice_data, dst_core)
    pipe_stddev_series = get_stddev_series(pipe_data, dst_core)
    unix_stddev_series = get_stddev_series(unix_data, dst_core)
    tcp_nd_stddev_series = get_stddev_series(tcp_nd_data, dst_core)
    tcp_stddev_series = get_stddev_series(tcp_data, dst_core)
  else:
    mempipe_spin_unsafe_stddev_series = None
    mempipe_spin_safe_stddev_series = None
    mempipe_futex_unsafe_stddev_series = None
    mempipe_futex_safe_stddev_series = None
    shmpipe_unsafe_stddev_series = None
    shmpipe_safe_stddev_series = None
    vmsplice_stddev_series = None
    pipe_stddev_series = None
    unix_stddev_series = None
    tcp_nd_stddev_series = None
    tcp_stddev_series = None

  ind = np.arange(n_groups)  # the x locations for the groups
  width = 0.8 / n_bars       # the width of the bars

#  subplot_id = str(len(cores) / 2) + str(2) + str(fig_idx)
#  print subplot_id
#  ax = fig.add_subplot(subplot_id)
  plt.clf()
  ax = fig.add_subplot(111)

  rects1 = ax.bar(ind, mempipe_spin_unsafe_series, width, color='r',
                  hatch='///', yerr=mempipe_spin_unsafe_stddev_series,
                  ecolor='k', label='mempipe-spin-unsafe', lw=0.5)
  rects2 = ax.bar(ind+width, mempipe_spin_safe_series, width, color='r',
                  yerr=mempipe_spin_safe_stddev_series, ecolor='k',
                  label='mempipe-spin-safe', lw=0.5)
  rects3 = ax.bar(ind+2*width, mempipe_futex_unsafe_series, width, color='g',
                  hatch='///', yerr=mempipe_futex_unsafe_stddev_series,
                  ecolor='k', label='mempipe-futex-unsafe', lw=0.5)
  rects4 = ax.bar(ind+3*width, mempipe_futex_safe_series, width, color='g',
                  yerr=mempipe_futex_safe_stddev_series, ecolor='k',
                  label='mempipe-futex-safe', lw=0.5)
  rects5 = ax.bar(ind+4*width, shmpipe_unsafe_series, width, color='b',
                  hatch='///', yerr=shmpipe_unsafe_stddev_series, ecolor='k',
                  label='shmempipe-unsafe', lw=0.5)
  rects6 = ax.bar(ind+5*width, shmpipe_safe_series, width, color='b',
                  yerr=shmpipe_safe_stddev_series, ecolor='k',
                  label='shmempipe-safe', lw=0.5)
  rects7 = ax.bar(ind+6*width, vmsplice_series, width, color='m',
                  yerr=vmsplice_stddev_series, ecolor='k',
                  label='vmsplice-coop', lw=0.5)
  rects8 = ax.bar(ind+7*width, pipe_series, width, color='y',
                  yerr=pipe_stddev_series, ecolor='k', label='pipe', lw=0.5)
  rects9 = ax.bar(ind+8*width, unix_series, width, color='k',
                  yerr=unix_stddev_series, ecolor='0.4', label='unix-sock', lw=0.5)
  rects10 = ax.bar(ind+9*width, tcp_nd_series, width, color='w',
                   yerr=tcp_nd_stddev_series, ecolor='k', label='tcp-nodelay', lw=0.5)
  rects11 = ax.bar(ind+10*width, tcp_series, width, color='c',
                   yerr=tcp_stddev_series, ecolor='k', label='tcp', lw=0.5)

  # add the subplot label
#  plt.text(0.33, 20000, labels[fig_idx - 1])

  # add some ticks
  ax.set_xticks(ind+(n_bars / 2 * width))
  ax.set_xticklabels( [ str(c) for c in chunksizes] )
#  ax.set_ylabel('Throughput [Mbps]')

  # set frame width to 0.5 for subplot
  [i.set_linewidth(0.5) for i in ax.spines.itervalues()]

  plt.ylim(0, all_max)

  plt.title("Core 0 to core " + str(dst_core))

#  if fig_idx == 1:
#    ax.set_ylabel('Throughput [Mbps]')
#    plt.legend(bbox_to_anchor=(-0.3, 1.04, 2.5, .102), loc=3,
#               ncol=3, mode="expand", borderaxespad=0.)
#    ax.legend( (rects1[0], rects2[0], rects3[0], rects4[0], rects5[0], rects6[0],
#                rects7[0], rects8[0], rects9[0], rects10[0], rects11[0]),
#                ('mempipe_spin_unsafe', 'mempipe_spin_safe', 'mempipe_futex_unsafe',
#                'mempipe_futex_safe', 'shmempipe_unsafe', 'shmempipe_safe',
#                'vmsplice_coop', 'pipe', 'unix', 'tcp', 'tcp_nodelay'), loc=2 )
#    leg = plt.gca().get_legend()
#    ltext = leg.get_texts()
#    lframe = leg.get_frame()
#    lframe.set_linewidth(0)
#    plt.setp(ltext, fontsize='medium')    # the legend text fontsize
#  elif fig_idx % 2 == 1:
#    pass
#  else:
#    ax.set_ylabel('')
#    ax.set_label('')
#    ax.set_yticks([], [])

  fig_idx = fig_idx + 1

  # Small version
  fig.set_size_inches(3,2)
  plt.savefig(output_dir + "/core_0_to_" + str(dst_core) + "-small.png",
              format="png")

  # Large version
  fig.set_size_inches(6,4)

  ax.set_ylabel('Throughput [Mbps]')
  ax.set_ylabel('Transfer size [bytes]')
  if not is_series:
    autolabel(rects1)
    autolabel(rects2)
    autolabel(rects3)
    autolabel(rects4)
    autolabel(rects5)
    autolabel(rects6)
    autolabel(rects7)
    autolabel(rects8)
    autolabel(rects9)
    autolabel(rects10)
    autolabel(rects11)

#  plt.savefig(output_dir + "/core_0_to_" + str(dst_core) + ".pdf",
#              format="pdf")
  plt.savefig(output_dir + "/core_0_to_" + str(dst_core) + ".png",
              format="png")

#mpl.rcParams['patch.linewidth'] = 0.5
#plt.subplots_adjust(left=0.15, right=1.0, top=0.8, bottom=0.05)
#plt.savefig("test.pdf", format="pdf")

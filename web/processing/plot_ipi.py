import sys, os

import numpy as np
import numpy.random
import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt
import matplotlib.cm as cm

def get_data_raw(filename):
  x, y, v = np.loadtxt(filename, usecols=(0,1,2), unpack=True)

  x_tmp = []
  y_tmp = []
  v_tmp = []
  retdata = {}
  j = 0
  for i in range(0, len(x)):
    if x[i] == 0 and y[i] == 1:
      # new series
      if i > 0:
        retdata[j] = [x_tmp, y_tmp, v_tmp]
#        print len(retdata[j][0])
        j = j + 1
      x_tmp = [x[i]]
      y_tmp = [y[i]]
      v_tmp = [v[i]]
    else:
      # continuing series
      x_tmp.append(x[i])
      y_tmp.append(y[i])
      v_tmp.append(v[i])
  print j
  return retdata

def get_data(filename):
  data = np.loadtxt(filename)
  return [data]

# ---------------------------
# Handle command line args

if len(sys.argv) < 4:
  print "usage: python plot_ipi.py <input file> <num cores> <title>"
  sys.exit(0)

input_file = sys.argv[1]
max_core_id = int(sys.argv[2])
title = str(sys.argv[3])

datasets = get_data(input_file)

i = 0
fig = plt.figure(figsize=(4,3))
for ds in datasets:
#  print ds
#  heatmap, xedges, yedges = np.histogram2d(ds[0], ds[1], bins=180, weights=ds[2])
#  extent = [xedges[0], xedges[-1], yedges[0], yedges[-1]]

  plt.clf()
#  print "plotting " + str(k)
#  plt.hexbin(ds[0], ds[1], C=ds[2], gridsize=max_core_id, linewidths=1, cmap=cm.jet, bins=None)
  plt.matshow(ds, fignum=0, vmin=1800, cmap="Greys")

#  plt.clf()
#  plt.imshow(heatmap, extent=extent)

  # add some
  plt.ylabel('Core ID')
  plt.ylim(0, max_core_id)
  plt.xlabel('Core ID')
  plt.xlim(0, max_core_id)
  plt.title(title)

  cb = plt.colorbar()
  cb.set_label('IPI latency in nanoseconds')

  plt.savefig("ipi_run_" + str(i) + ".pdf", format="pdf", bbox_inches='tight')
  plt.savefig("ipi_run_" + str(i) + ".png", format="png", bbox_inches='tight')

  i = i + 1

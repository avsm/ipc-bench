import sys, os

import matplotlib
matplotlib.use("Agg")
import numpy as np
import numpy.random
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import pylab

def get_data(filename):
  data = np.loadtxt(filename)

  x_tmp = []
  y_tmp = []
  v_tmp = []
  retdata = {}
  for i in range(0, len(data)):
    for j in range(0, len(data[i])):
      x_tmp.append(i)
      y_tmp.append(j)
      v_tmp.append(data[i][j])

  retdata = [x_tmp, y_tmp, v_tmp]
  print len(retdata)
  return retdata


# ---------------------------
# Handle command line args

if len(sys.argv) < 2:
  print "usage: python plot_lat.py <input file> <title> [fix-scale]"
  sys.exit(0)

input_file = sys.argv[1]

fix_scale = 0
if len(sys.argv) > 3:
  fix_scale = int(sys.argv[3])

raw_data = np.loadtxt(input_file)
data = get_data(input_file)

fig = plt.figure(figsize=(4,3))
#f = pylab.Figure(figsize=(2,1.5))
#  print ds
#heatmap, xedges, yedges = np.histogram2d(data[0], data[1], bins=48, weights=data[2])
#extent = [xedges[0], xedges[-1], yedges[0], yedges[-1]]

#plt.clf()
#plt.hexbin(data[0], data[1], C=data[2], gridsize=40, linewidths=1, cmap=cm.jet, bins=None)

#plt.clf()
#plt.imshow(heatmap, extent=extent)

if fix_scale != 0:
  plt.matshow(raw_data, vmax=0.0001, vmin=0.00001, fignum=0)
else:
  plt.matshow(raw_data, fignum=0)

# add some
plt.ylabel('Core ID')
plt.ylim(0, 48)
plt.xlabel('Core ID')
plt.xlim(0, 48)
plt.title(sys.argv[2])

cb = plt.colorbar(shrink=1.0, format='%.3e')
cb.set_label('Latency in microseconds')

#plt.savefig("lat_" + sys.argv[1] + ".pdf", format="pdf", bbox_inches='tight')
plt.savefig(out_dir + "/lat_" + sys.argv[1] + ".png", format="png", bbox_inches='tight')

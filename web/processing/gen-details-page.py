#!/usr/env/python
import sys, os
import re
import subprocess

data_dir = sys.argv[1]
results_dir = data_dir + "/results"
out_dir = sys.argv[2]
target_cpus = sys.argv[3]
name = sys.argv[4]
outfile = out_dir + "/" + name + ".html"

github_user = "ms705"

print "Generating details page for %s" % name
#print "Target CPUs are: %s" % target_cpus.split(",")

processor_ids = []
model_names = []
for line in open(data_dir + "/logs/cpuinfo").readlines():
  r = re.search("processor\t: ([0-9]+)", line)
  if r:
    processor_ids.append(r.group(1))
  r = re.search("model name\t: (.+)", line)
  if r:
    model_names.append(r.group(1))

num_cores = len(processor_ids)

# NUMA-ness & number of nodes
numa_string = "unknown"
try:
  l = os.listdir(data_dir + "/logs/sys-node")
  if len(l) > 1:
    numa_string = "yes, %d nodes" % len(l)
  else:
    numa_string = "no"
except:
  pass

# Memory
mem_string = "unknown"
for line in open(data_dir + "/logs/meminfo").readlines():
  r = re.search("MemTotal:[ ]+([0-9]+) kB", line)
  if r:
    mem_string = str(round((float(r.group(1)) / float(1024*1024)), 2)) + " GB"

# virtualization
virtualized_string = "unknown"
for line in open(data_dir + "/logs/dmesg_virt").readlines():
  r = re.search("bare hardware", line)
  if r:
    virtualized_string = "no"
    break
  r = re.search("(Xen|virtual hardware|virtualized system)", line)
  # need to differentiate between different forms of virtualization
  if r:
    virtualized_string = "yes"
    break

# uname/OS
os_string = "unknown"
for line in open(data_dir + "/logs/uname").readlines():
  fields = line.split()
  os_id = fields[0]
  ver = fields[1]
  arch = fields[2]
  os_string = "%s %s, %s" % (os_id, ver, arch)

# Generate latency graphs
lat_ls = None
try:
  lat_ls = os.listdir(results_dir + "/lat")
  for lat_file in lat_ls:
    if not re.search("\.csv", lat_file):
      continue
    argv = ["python", "plot_lat.py", results_dir + "/lat/" + lat_file, \
            lat_file, data_dir + "/graphs", str(len(processor_ids))]
    subprocess.check_call(argv)
except:
  pass

# Generate throughput graphs
argv = ["python", "plot_thr.py", results_dir, target_cpus, "0"]
subprocess.check_call(argv)

out_html = "<p style=\"background-color: lightgray;\"><b>" \
    "<a href=\"../results.html\">&laquo; Back to overview</a></b></p>"

# Generate hardware overview section
html = "<h2>Hardware overview</h2>"

hw_string = "<table>"
row_string = "<tr><td>%s</td><td>%s</td></tr>"
hw_string = hw_string + row_string \
    % ("Cores (threads):", (str(len(processor_ids)) + ", " + model_names[0]))
hw_string = hw_string + row_string % ("NUMA:", numa_string)
hw_string = hw_string + row_string % ("Total memory:", mem_string)
hw_string = hw_string + row_string % ("Operating system:", os_string)
hw_string = hw_string + row_string % ("Virtualized:", virtualized_string)
hw_string = hw_string + "</table>"

html = html + hw_string
out_html = out_html + html

# Generate latency heatmap section
html = "<h2>Latency</h2>"
html = html + "<p>These graphs show the pairwise IPC latency between cores."
html = html + "<table><tr>"
thr_graphs_links = ""
i = 0
graphs_per_row = 3
if lat_ls is None:
  html = html + "<td><b><em>No data.</em></b></td>"
else:
  for t in lat_ls:
    if not re.search("\.csv", t):
      continue
    graph_file = "../graphs/%s/lat_%s.png" % (name, t)
    if i % graphs_per_row == 0:
      html = html + "</tr><tr>"
    html = html + "<td><a href=\"%s\"><img src=\"%s\" /></a></td>" \
        % (graph_file, graph_file)
    i = i + 1

html = html + "</tr></table>"
out_html = out_html + html

# Generate throughput graphs section
html = "<h2>Throughput</h2>"
html = html + "<p>These graphs show the IPC throughput for continous " \
    "communication between a pair of cores. The y-axis shows throughput in " \
    "Gbps, and the x-axis different chunk sizes.<br />" \
    "<b>Click on the graphs to show a larger version.</b></p>" \
    "<p><img src=\"../images/thr_legend.png\" class=\"aleft\" " \
    "style=\"border: none;\" /></p>"
html = html + "<table><tr>"
thr_graphs_links = ""
i = 0
graphs_per_row = 4
for c in target_cpus.split(","):
  graph_file = "../graphs/%s/core_0_to_%s.png" % (name, c)
  thumb_file = "../graphs/%s/core_0_to_%s-small.png" % (name, c)
  if i % graphs_per_row == 0:
    html = html + "</tr><tr>"
  html = html + "<td><a href=\"%s\"><img src=\"%s\" /></a></td>" \
      % (graph_file, thumb_file)
  i = i + 1

#  if thr_graphs_links != "":
#    thr_graphs_links = thr_graphs_links + ", "
#  thr_graphs_links = thr_graphs_links + "<a href=\"%s\">0 to %s</a>" \
#      % (graph_file, c)

html = html + "</tr></table>"
out_html = out_html + html


# Raw data link section
html = "<h2>Raw data</h2><p>The raw results data for this experiment can " \
    "be downloaded "
raw_data_link = "<a href=\"https://raw.github.com/%s/ipc-bench/master/" \
    "results/%s.tar.gz\">here</a>" % (github_user, name)
html = html + raw_data_link + ".</p>"

out_html = out_html + html

out_html = out_html + "<p style=\"background-color: lightgray;\"><b>" \
    "<a href=\"../results.html\">&laquo; Back to overview</a></b></p>"

out_fd = open(outfile, "a")
out_fd.write(out_html)
out_fd.close()

#!/usr/env/python
import sys, os
import re
import subprocess

data_dir = sys.argv[1]
results_dir = data_dir + "/results"
outfile = sys.argv[2]
target_cpus = sys.argv[3]
name = sys.argv[4]

github_user = "ms705"

print "Processing data in %s" % data_dir
print "Target CPUs are: %s" % target_cpus.split(",")

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
    numa_string = "yes (%d)" % len(l)
  else:
    numa_string = "no"
except:
  pass

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
  os = fields[0]
  ver = fields[1]
  arch = fields[2]
  os_string = "%s %s, %s" % (os, ver, arch)

# Generate HTML output
html = "<tr><td>%s</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td>" \
    "<td>%s</td><td>%s</td></tr>"

thr_graphs_links = ""
for c in target_cpus.split(","):
  graph_file = "graphs/%s/core_0_to_%s.png" % (name, c)
  if thr_graphs_links != "":
    thr_graphs_links = thr_graphs_links + ", "
  thr_graphs_links = thr_graphs_links + "<a href=\"%s\">0 to %s</a>" \
      % (graph_file, c)

details_link = "<a href=\"details/%s.html\">View</a>" % name

raw_data_link = "<a href=\"https://raw.github.com/%s/ipc-bench/master/" \
    "results/%s.tar.gz\">Download</a>" % (github_user, name)

out_html = html % (model_names[0], num_cores, numa_string, os_string,
                   virtualized_string, details_link, raw_data_link)

out_fd = open(outfile, "a")
out_fd.write(out_html)
out_fd.close()

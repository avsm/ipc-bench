#!/usr/bin/python

import sys
import os
import os.path
import tempfile
import subprocess
import shutil
import re

cpu_regex = re.compile("^cpu(\d+)$")
index_regex = re.compile("^index(\d+)$")
node_regex = re.compile("^node(\d+)$")

index_files = ["coherency_line_size", "level", "number_of_sets", "physical_line_partition",
	       "shared_cpu_list", "size", "type", "ways_of_associativity"]

topology_files = ["core_id", "core_siblings_list", "physical_package_id", "thread_siblings_list"]

node_files = ["cpulist", "distance", "meminfo"]

# Support function: parses Linux kernel bitmap-lists into lists of integers

def parse_list(s):

	result = []

	bits = s.split(",")
	for bit in bits:
		nums = bit.split("-")
		if len(nums) == 1:
			result.append(int(nums[0]))
		elif len(nums) == 2:
			result.extend(range(int(nums[0]), int(nums[1]) + 1))
		else:
			raise Exception("Unexpected format for bitmap list (split on - yielded %s)" % nums)

	return result

tempdir = tempfile.mkdtemp()
logdir = os.path.join(tempdir, "logs")
resultdir = os.path.join(tempdir, "results")

print "Writing output to", tempdir

os.mkdir(logdir)
os.mkdir(resultdir)

with open(os.path.join(logdir, "uname"), "w") as f:
	subprocess.check_call(["uname", "-s", "-r", "-m", "-p", "-i", "-o"], stdout=f)

shutil.copyfile("/proc/cpuinfo", os.path.join(logdir, "cpuinfo"))
shutil.copyfile("/proc/meminfo", os.path.join(logdir, "meminfo"))

with open(os.path.join(logdir, "dmesg_virt"), "w") as f:
	subprocess.call(["bash", "-c", "dmesg | grep -i virtual"], stdout=f)

n_cpus = 0
cpu_sockets = dict()
cpu_ht_partners = dict()
cpu0_shared_caches = dict() # Maps each CPU number to the lowest level of cache it shares with 0.

cpu_dirs = None
try:
	cpu_dirs = os.listdir("/sys/devices/system/cpu")
except:
	print >>sys.stderr, "Can't read /sys/devices/system/cpu"

if cpu_dirs is not None:
	copy_dir = os.path.join(logdir, "sys-cpu")
	os.mkdir(copy_dir)
	for d in cpu_dirs:
		re_match = cpu_regex.match(d)
		if re_match is not None:
			this_cpu_id = int(re_match.group(1))
			n_cpus += 1
			dest_dir = os.path.join(copy_dir, d)
			os.mkdir(dest_dir)
			cache_dest_dir = os.path.join(dest_dir, "cache")
			os.mkdir(cache_dest_dir)
			cache_source_dir = os.path.join("/sys/devices/system/cpu", d, "cache")
			for cache_d in os.listdir(cache_source_dir):
				index_match = index_regex.match(cache_d)
				if index_match is not None:
					index_source_dir = os.path.join(cache_source_dir, cache_d)
					index_dest_dir = os.path.join(cache_dest_dir, cache_d)
					os.mkdir(index_dest_dir)
					for f in index_files:
						try:
							shutil.copyfile(os.path.join(index_source_dir, f),
									os.path.join(index_dest_dir, f))
						except:
							print >>stderr, "Couldn't copy", os.path.join(index_source_dir, f)
					if this_cpu_id == 0:
						with open(os.path.join(index_source_dir, "shared_cpu_list"), "r") as f:
							this_sharing_cpus = parse_list(f.read())
						with open(os.path.join(index_source_dir, "level"), "r") as f:
							this_level = int(f.read())
						for cpu in this_sharing_cpus:
							if cpu not in cpu0_shared_caches or cpu0_shared_caches[cpu] > this_level:
								cpu0_shared_caches[cpu] = this_level	
			topology_dir = os.path.join(dest_dir, "topology")
			topology_from_dir = os.path.join("/sys/devices/system/cpu", d, "topology")
			os.mkdir(topology_dir)
			for f in topology_files:
				copy_from = os.path.join(topology_from_dir, f)
				copy_to = os.path.join(topology_dir, f)
				shutil.copyfile(copy_from, copy_to)
			with open(os.path.join(topology_from_dir, "physical_package_id"), "r") as f:
				cpu_sockets[this_cpu_id] = int(f.read())
			with open(os.path.join(topology_from_dir, "thread_siblings_list"), "r") as f:
				cpu_ht_partners[this_cpu_id] = parse_list(f.read())
				
node_dirs = None
try:
	node_dirs = os.listdir("/sys/devices/system/node")
except:
	print >>sys.stderr, "Can't read /sys/devices/system/node"

n_nodes = 0
cpu_nodes = dict()

if node_dirs is not None:
	dest_dir = os.path.join(logdir, "sys-node")
	os.mkdir(dest_dir)
	for d in node_dirs:
		re_match = node_regex.match(d)
		if re_match is not None:
			n_nodes += 1
			this_node = int(re_match.group(1))
			node_source_dir = os.path.join("/sys/devices/system/node", d)
			node_dest_dir = os.path.join(dest_dir, d)
			os.mkdir(node_dest_dir)
			for f in node_files:
				copy_from = os.path.join(node_source_dir, f)
				copy_to = os.path.join(node_dest_dir, f)
				try:
					shutil.copyfile(copy_from, copy_to)
				except:
					print >>sys.stderr, "Couldn't copy", copy_from
			with open(os.path.join(node_source_dir, "cpulist"), "r") as f:
				node_cpus = parse_list(f.read())
				for cpu in node_cpus:
					cpu_nodes[cpu] = this_node

for n in range(n_cpus):
	if n not in cpu0_shared_caches:
		cpu0_shared_caches[n] = "memory"

if n_cpus == 0:
	print "Don't know how many CPUs we have, assuming 1"
	n_cpus = 1
	cpu_sockets[0] = 0
	cpu_ht_partners[0] = [0]
	
if n_nodes == 0:
	print "Don't know how many NUMA nodes we have, assuming 1 (UMA)"
	n_nodes = 1
	for x in range(n_cpus):
		cpu_nodes[x] = 0

# If there are few CPUs, test all combinations.
# If not, target the CPU's HT sibling if it exists.
# Then target a sample of each unique (numa-node, physical-socket, lowest-shared-cache-level) triple.

print "Detected system topology:"

for i in range(n_cpus):
	print "CPU %d: socket %d, node %d, best CPU0 shared cache level: %s" % (i, cpu_sockets[i], cpu_nodes[i], cpu0_shared_caches[i])

target_cpus = []

if n_cpus <= 4:
	for i in range(n_cpus):
		target_cpus.append(i)
else:
	target_cpus = [0]
	# If we've got an HT partner, target that
	partners = cpu_ht_partners[0]
	if len(partners) > 1:
		target_cpus.append(partners[1])
	origin_loc = (cpu_sockets[0], cpu_nodes[0], 1)
	# Find an example for each unique (socket, numa node, shared cache) available.
	taken_locs = set()
	for i in range(1, n_cpus):
		this_loc = (cpu_sockets[i], cpu_nodes[i], cpu0_shared_caches[i])
		if this_loc in taken_locs:
			continue
		elif this_loc == origin_loc and i in target_cpus:
			continue
		else:
			taken_locs.add(this_loc)
			target_cpus.append(i)

print "Testing using target CPUs:", target_cpus

# Write record of target CPUs to file for easier post-processing
f = open(os.path.join(logdir, "target_cpus"), 'w+')
f.write(",".join([str(x) for x in target_cpus]))
f.close()

target_cpus_nodes = []
zero_node = cpu_nodes[0]

for i in target_cpus:
	target_node = cpu_nodes[i]
	if target_node == zero_node:
		target_cpus_nodes.extend([i, zero_node])
	else:
		target_cpus_nodes.extend([i, zero_node, i, target_node])

try:
	subprocess.check_call(["make"])
except:
	print >>sys.stderr, "Make failed -- check you have libnuma headers and libraries available"
	sys.exit(1)

try:
	print >>sys.stderr, "Running latency tests... (over %d CPUs)" % n_cpus
	argv = ["./all_lat.sh", resultdir + "/lat", str(n_cpus)]
	print argv
	subprocess.check_call(argv)
except:
	print >>sys.stderr, "At least one latency test failed. See output above for more detail."
	sys.exit(1)

try:
	print >>sys.stderr, "Running throughput tests..."
	argv = ["./all_thr.py", resultdir]
	argv.extend([str(i) for i in target_cpus_nodes])
	subprocess.check_call(argv)
except:
	print >>sys.stderr, "At least one throughput test failed. See output above for more detail."
	sys.exit(1)

out_file = "%s.tar.gz" % tempdir
subprocess.check_call(["/bin/tar", "cvfz", out_file, tempdir])

print "Test succeeded. Output written as", out_file
print "Please email that file to cl-ipc-bench@lists.cam.ac.uk"

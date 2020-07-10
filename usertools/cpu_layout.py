#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2017 Cavium, Inc. All rights reserved.

from __future__ import print_function
import sys
try:
    xrange # Python 2
except NameError:
    xrange = range # Python 3

if sys.version_info.major < 3:
    print("WARNING: Python 2 is deprecated for use in DPDK, and will not work in future releases.", file=sys.stderr)
    print("Please use Python 3 instead", file=sys.stderr)

sockets = []
cores = []
core_map = {}
base_path = "/sys/devices/system/cpu"
fd = open("{}/kernel_max".format(base_path))
max_cpus = int(fd.read())
fd.close()
for cpu in xrange(max_cpus + 1):
    try:
        fd = open("{}/cpu{}/topology/core_id".format(base_path, cpu))
    except IOError:
        continue
    except:
        break
    core = int(fd.read())
    fd.close()
    fd = open("{}/cpu{}/topology/physical_package_id".format(base_path, cpu))
    socket = int(fd.read())
    fd.close()
    if core not in cores:
        cores.append(core)
    if socket not in sockets:
        sockets.append(socket)
    key = (socket, core)
    if key not in core_map:
        core_map[key] = []
    core_map[key].append(cpu)

print(format("=" * (47 + len(base_path))))
print("Core and Socket Information (as reported by '{}')".format(base_path))
print("{}\n".format("=" * (47 + len(base_path))))
print("cores = ", cores)
print("sockets = ", sockets)
print("")

max_processor_len = len(str(len(cores) * len(sockets) * 2 - 1))
max_thread_count = len(list(core_map.values())[0])
max_core_map_len = (max_processor_len * max_thread_count)  \
                      + len(", ") * (max_thread_count - 1) \
                      + len('[]') + len('Socket ')
max_core_id_len = len(str(max(cores)))

output = " ".ljust(max_core_id_len + len('Core '))
for s in sockets:
    output += " Socket %s" % str(s).ljust(max_core_map_len - len('Socket '))
print(output)

output = " ".ljust(max_core_id_len + len('Core '))
for s in sockets:
    output += " --------".ljust(max_core_map_len)
    output += " "
print(output)

for c in cores:
    output = "Core %s" % str(c).ljust(max_core_id_len)
    for s in sockets:
        if (s,c) in core_map:
            output += " " + str(core_map[(s, c)]).ljust(max_core_map_len)
        else:
            output += " " * (max_core_map_len + 1)
    print(output)

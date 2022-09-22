#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021 PANTHEON.tech s.r.o.

import ctypes
import glob
import os
import subprocess
import re

if os.name == 'posix':
    if os.path.isdir('/sys/devices/system/node'):
        numa_nodes = glob.glob('/sys/devices/system/node/node*')
        numa_nodes.sort(key=lambda l: int(re.findall('\d+', l)[0]))
        print(int(os.path.basename(numa_nodes[-1])[4:]) + 1)
    else:
        subprocess.run(['sysctl', '-n', 'vm.ndomains'], check=False)

elif os.name == 'nt':
    libkernel32 = ctypes.windll.kernel32

    numa_count = ctypes.c_ulong()

    libkernel32.GetNumaHighestNodeNumber(ctypes.pointer(numa_count))
    print(numa_count.value + 1)

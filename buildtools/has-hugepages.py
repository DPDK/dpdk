# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021 Microsoft Corporation
"""This script checks if the system supports huge pages"""

import platform
import ctypes

os_name = platform.system()
if os_name == "Linux":
    try:
        with open("/proc/sys/vm/nr_hugepages") as file_o:
            content = file_o.read()
            print(content)
    except:
        print("0")

elif os_name == "FreeBSD":
    # Assume FreeBSD always has hugepages enabled
    print("1")
elif os_name == "Windows":
    if ctypes.windll.kernel32.GetLargePageMinimum() > 0:
        print("1")
    else:
        print("0")
else:
    print("0")

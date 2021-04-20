#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

import os
import sys
import shutil
from os.path import abspath, dirname, join

def fixup_library_renames(contents):
    """since library directory names have dropped the 'librte_' prefix,
    add those prefixes back in for patches than need it"""
    modified = False

    # first get all the DPDK libs to build up replacement list
    # stored in function attribute between calls
    try:
        libdirs = fixup_library_renames.libdirs
    except AttributeError:
        dpdk_libdir = abspath(join(dirname(sys.argv[0]), '..', 'lib'))
        for root, dirs, files in os.walk(dpdk_libdir):
            fixup_library_renames.libdirs = dirs
            libdirs = dirs
            break

    for i in range(len(contents)):
        # skip over any lines which don't have lib in it
        if not "lib/" in contents[i]:
            continue
        for d in libdirs:
            if f'lib/{d}' in contents[i]:
                modified = True
                contents[i] = contents[i].replace(f'lib/{d}', f'lib/librte_{d}')
    return modified

def main():
    "takes list of patches off argv and processes each"
    for patch in sys.argv[1:]:
        modified = False
        with open(patch) as f:
            contents = f.readlines()

        modified |= fixup_library_renames(contents)
        # other functions to change the patch go here

        if not modified:
            continue
        shutil.copyfile(f'{patch}', f'{patch}.bak')
        with open(patch, 'w') as f:
            f.writelines(contents)

if __name__ == "__main__":
    main()

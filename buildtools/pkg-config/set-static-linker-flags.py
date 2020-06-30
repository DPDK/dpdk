#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation

# Script to fix flags for static linking in pkgconfig files from meson
# Should be called from meson build itself
import os
import sys


def fix_ldflag(f):
    if not f.startswith('-lrte_'):
        return f
    return '-l:lib' + f[2:] + '.a'


def fix_libs_private(line):
    if not line.startswith('Libs.private'):
        return line
    ldflags = [fix_ldflag(flag) for flag in line.split()]
    return ' '.join(ldflags) + '\n'


def process_pc_file(filepath):
    print('Processing', filepath)
    with open(filepath) as src:
        lines = src.readlines()
    with open(filepath, 'w') as dst:
        dst.writelines([fix_libs_private(line) for line in lines])


if 'MESON_BUILD_ROOT' not in os.environ:
    print('This script must be called from a meson build environment')
    sys.exit(1)
for root, dirs, files in os.walk(os.environ['MESON_BUILD_ROOT']):
    pc_files = [f for f in files if f.endswith('.pc')]
    for f in pc_files:
        process_pc_file(os.path.join(root, f))

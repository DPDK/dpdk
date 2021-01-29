#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

from sys import argv
from os.path import abspath

(h_file, c_file) = argv[1:]

contents = '#include "' + abspath(h_file) + '"'
with open(c_file, 'w') as cf:
    cf.write(contents)

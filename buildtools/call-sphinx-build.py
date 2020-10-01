#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation
#

import sys
import os
from os.path import join
from subprocess import run, PIPE, STDOUT
from distutils.version import StrictVersion

# assign parameters to variables
(sphinx, version, src, dst, *extra_args) = sys.argv[1:]

# set the version in environment for sphinx to pick up
os.environ['DPDK_VERSION'] = version

# for sphinx version >= 1.7 add parallelism using "-j auto"
ver = run([sphinx, '--version'], stdout=PIPE,
          stderr=STDOUT).stdout.decode().split()[-1]
sphinx_cmd = [sphinx] + extra_args
if StrictVersion(ver) >= StrictVersion('1.7'):
    sphinx_cmd += ['-j', 'auto']

# find all the files sphinx will process so we can write them as dependencies
srcfiles = []
for root, dirs, files in os.walk(src):
    srcfiles.extend([join(root, f) for f in files])

# run sphinx, putting the html output in a "html" directory
with open(join(dst, 'sphinx_html.out'), 'w') as out:
    process = run(sphinx_cmd + ['-b', 'html', src, join(dst, 'html')],
                  stdout=out)

# create a gcc format .d file giving all the dependencies of this doc build
with open(join(dst, '.html.d'), 'w') as d:
    d.write('html: ' + ' '.join(srcfiles) + '\n')

sys.exit(process.returncode)

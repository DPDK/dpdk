#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation
#

import sys
import os
from os.path import join
from subprocess import run, PIPE
from distutils.version import StrictVersion

(sphinx, src, dst) = sys.argv[1:]  # assign parameters to variables

# for sphinx version >= 1.7 add parallelism using "-j auto"
ver = run([sphinx, '--version'], stdout=PIPE).stdout.decode().split()[-1]
sphinx_cmd = [sphinx]
if StrictVersion(ver) >= StrictVersion('1.7'):
    sphinx_cmd += ['-j', 'auto']

# find all the files sphinx will process so we can write them as dependencies
srcfiles = []
for root, dirs, files in os.walk(src):
    srcfiles.extend([join(root, f) for f in files])

# run sphinx, putting the html output in a "html" directory
process = run(sphinx_cmd + ['-b', 'html', src, join(dst, 'html')], check=True)
print(str(process.args) + ' Done OK')

# create a gcc format .d file giving all the dependencies of this doc build
with open(join(dst, '.html.d'), 'w') as d:
    d.write('html: ' + ' '.join(srcfiles) + '\n')

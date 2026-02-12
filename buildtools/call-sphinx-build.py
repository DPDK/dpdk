#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation
#

import filecmp
import shutil
import sys
import os
from os.path import join
from subprocess import run

# assign parameters to variables
(sphinx, version, src, dst, *extra_args) = sys.argv[1:]

# set the version in environment for sphinx to pick up
os.environ['DPDK_VERSION'] = version
if 'dts' in src:
    os.environ['DTS_DOC_BUILD'] = "y"

sphinx_cmd = [sphinx] + extra_args

# find all the files sphinx will process so we can write them as dependencies
srcfiles = []
for root, dirs, files in os.walk(src):
    srcfiles.extend([join(root, f) for f in files])

# create destination path if not already present
os.makedirs(dst, exist_ok=True)

# run sphinx, putting the html output in a "html" directory
with open(join(dst, 'sphinx_html.out'), 'w') as out:
    # don't append html dir if dst is already nested in a html dir
    last_two_dirs = os.path.join(*os.path.normpath(dst).split(os.path.sep)[-2:])
    html_dst = dst if 'html' in last_two_dirs else join(dst, 'html')
    process = run(sphinx_cmd + ['-b', 'html', src, html_dst], stdout=out)

# create a gcc format .d file giving all the dependencies of this doc build
with open(join(dst, '.html.d'), 'w') as d:
    d.write('html: ' + ' '.join(srcfiles) + '\n')

# copy custom CSS file
css = 'custom.css'
src_css = join(src, css)
dst_css = join(html_dst, '_static', 'css', css)
if not os.path.exists(dst_css) or not filecmp.cmp(src_css, dst_css):
    os.makedirs(os.path.dirname(dst_css), exist_ok=True)
    shutil.copyfile(src_css, dst_css)

sys.exit(process.returncode)

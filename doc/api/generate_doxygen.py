#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# (c) 2018 Luca Boccassi <bluca@debian.org>
# (c) 2022 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import os, re, subprocess, sys

pattern = re.compile('^Preprocessing (.*)...$')
out_dir, *doxygen_command = sys.argv[1:]
out_file = os.path.join(out_dir + '.out')
dep_file = f'{out_dir}.d'
with open(out_file, 'w') as out:
    subprocess.run(doxygen_command, check=True, stdout=out)
with open(out_file) as out, open(dep_file, 'w') as dep:
    print(f'{out_dir}:', end=' ', file=dep)
    for line in sorted(out):
        match = re.match(pattern, line)
        if match:
            print(match.group(1), end=' ', file=dep)

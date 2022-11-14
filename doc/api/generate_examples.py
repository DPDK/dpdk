#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# (c) 2018 Luca Boccassi <bluca@debian.org>
# (c) 2022 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import os, sys

examples_dir, api_examples = sys.argv[1:]

sources = []
with open(f'{api_examples}.d', 'w') as dep:
    print(f'{api_examples}:', end=' ', file=dep)
    for root, _, files in os.walk(examples_dir):
        for name in sorted(files):
            is_source = name.endswith('.c')
            if is_source or name == 'meson.build':
                path = os.path.join(root, name)
                if is_source:
                    sources.append(path)
                print(path , end=' ', file=dep)

with open(api_examples, 'w') as out:
    print('''/**
@page examples DPDK Example Programs
''', file=out)
    for path in sorted(sources):
        # Produce consistent output with forward slashes on all systems.
        # Every \ in paths within examples directory is a separator, not escape.
        relpath = os.path.relpath(path, examples_dir).replace('\\', '/')
        print(f'@example examples/{relpath}', file=out)
    print('*/', file=out)

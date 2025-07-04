#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Intel Corporation

import os
import re
import sys
from os.path import dirname, realpath, join

buildtools_path = dirname(realpath(__file__))
basedir = dirname(buildtools_path)

with open(join(basedir, "meson.build")) as f:
    keyword = "meson_version_windows" if os.name == "nt" else "meson_version"
    pattern = fr"{keyword}:\s*'>=\s*([0-9\.]+)'"

    for ln in f.readlines():
        if keyword not in ln:
            continue

        ln = ln.strip()
        ver_match = re.search(pattern, ln)
        if not ver_match:
            print(
                f"Meson version specifier not in recognised format: '{ln}'",
                file=sys.stderr,
            )
            sys.exit(1)
        print(ver_match.group(1), end="")
        break

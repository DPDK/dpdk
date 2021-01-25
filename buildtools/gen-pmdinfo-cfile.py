#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2020 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import os
import subprocess
import sys
import tempfile

_, ar, archive, output, *pmdinfogen = sys.argv
with tempfile.TemporaryDirectory() as temp:
    proc = subprocess.run(
        # Don't use "ar p", because its output is corrupted on Windows.
        [ar, "xv", os.path.abspath(archive)], stdout=subprocess.PIPE, check=True, cwd=temp
    )
    lines = proc.stdout.decode().splitlines()
    names = [line[len("x - ") :] for line in lines]
    paths = [os.path.join(temp, name) for name in names]
    subprocess.run(pmdinfogen + paths + [output], check=True)

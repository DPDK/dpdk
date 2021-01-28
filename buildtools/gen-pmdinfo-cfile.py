#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2020 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import os
import subprocess
import sys
import tempfile

_, tmp_root, ar, archive, output, *pmdinfogen = sys.argv
with tempfile.TemporaryDirectory(dir=tmp_root) as temp:
    run_ar = lambda command: subprocess.run(
        [ar, command, os.path.abspath(archive)],
        stdout=subprocess.PIPE, check=True, cwd=temp
    )
    # Don't use "ar p", because its output is corrupted on Windows.
    run_ar("x")
    names = run_ar("t").stdout.decode().splitlines()
    paths = [os.path.join(temp, name) for name in names]
    subprocess.run(pmdinfogen + paths + [output], check=True)

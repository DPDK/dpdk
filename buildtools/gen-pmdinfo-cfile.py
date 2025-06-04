#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2020 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import os
import subprocess
import sys

_, archiver, tmp_dir, archive, output, *pmdinfogen = sys.argv
paths = []
if archiver == "lib":
    archiver_options = ["/LIST", "/NOLOGO"]
else:
    archiver_options = ["t"]

for name in (
    subprocess.run(
        [archiver] + archiver_options + [archive],
        stdout=subprocess.PIPE,
        check=True,
    )
    .stdout.decode()
    .splitlines()
):
    if os.path.exists(name):
        paths.append(name)
    else:
        if not os.path.exists(tmp_dir):
            os.makedirs(tmp_dir)
        if archiver == "lib":
            run_args = [archiver, f"/EXTRACT:{name}", os.path.abspath(archive)]
        else:
            run_args = [archiver, "x", os.path.abspath(archive), name]
        subprocess.run(run_args, check=True, cwd=tmp_dir)
        paths.append(os.path.join(tmp_dir, name))
subprocess.run(pmdinfogen + paths + [output], check=True)

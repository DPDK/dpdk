#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2020 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import os
import subprocess
import sys

_, ar, tmp_dir, archive, output, *pmdinfogen = sys.argv
paths = []
for name in subprocess.run([ar, "t", archive], stdout=subprocess.PIPE,
                            check=True).stdout.decode().splitlines():
    if os.path.exists(name):
        paths.append(name)
    else:
        if not os.path.exists(tmp_dir):
            os.makedirs(tmp_dir)
        subprocess.run([ar, "x", os.path.abspath(archive), name],
                        check=True, cwd=tmp_dir)
        paths.append(os.path.join(tmp_dir, name))
subprocess.run(pmdinfogen + paths + [output], check=True)

#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation

import subprocess
import sys
import tempfile

objdump, *cc = sys.argv[1:]
with tempfile.NamedTemporaryFile() as obj:
    # On Windows, the file is opened exclusively and is not writable.
    obj.close()
    # from https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90028
    gather_params = '0x8(,%ymm1,1),%ymm0{%k2}'
    src = '__asm__("vpgatherqq {}");'.format(gather_params).encode('utf-8')
    subprocess.run(cc + ['-c', '-xc', '-o', obj.name, '-'], input=src, check=True)
    asm = subprocess.run([objdump, '-d', '--no-show-raw-insn', obj.name],
                         stdout=subprocess.PIPE, check=True).stdout.decode('utf-8')
    if gather_params not in asm:
	    print('vpgatherqq displacement error with as')
	    sys.exit(1)

# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2022 Marvell.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files(
        'test_security_perf.c',
        '../test/test_cryptodev_security_ipsec.c',
)
deps += ['security', 'cmdline']

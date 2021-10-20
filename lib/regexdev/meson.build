# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020 Mellanox Technologies, Ltd

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('rte_regexdev.c')
headers = files('rte_regexdev.h', 'rte_regexdev_driver.h')
indirect_headers += files('rte_regexdev_core.h')
deps += ['mbuf']

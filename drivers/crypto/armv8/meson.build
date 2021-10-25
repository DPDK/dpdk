# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Arm Limited

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

dep = dependency('libAArch64crypto', required: false, method: 'pkg-config')
if not dep.found()
    build = false
    reason = 'missing dependency, "libAArch64crypto"'
    subdir_done()
endif

ext_deps += dep
deps += ['bus_vdev']
sources = files('rte_armv8_pmd.c', 'rte_armv8_pmd_ops.c')

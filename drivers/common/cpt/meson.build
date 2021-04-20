# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

sources = files('cpt_fpm_tables.c', 'cpt_pmd_ops_helper.c')

deps = ['kvargs', 'pci', 'cryptodev']
includes += include_directories('../../crypto/octeontx')

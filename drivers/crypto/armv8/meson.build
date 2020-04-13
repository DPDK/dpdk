# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Arm Limited

path = get_option('armv8_crypto_dir')
if path == ''
	build = false
	reason = 'missing dependency, "armv8_crypto"'
	subdir_done()
endif

inc_dir = path

lib = cc.find_library('libAArch64crypto', dirs: [path], required: false)
if not lib.found()
	build = false
	reason = 'missing dependency, "AArch64crypto"'
	subdir_done()
else
	ext_deps += lib
	includes += include_directories(inc_dir)
endif

deps += ['bus_vdev']
sources = files('rte_armv8_pmd.c', 'rte_armv8_pmd_ops.c')

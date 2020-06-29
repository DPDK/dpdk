# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
	subdir_done()
endif

pmdinfogen_inc = [global_inc]
pmdinfogen_inc += include_directories('../../lib/librte_eal/include')
pmdinfogen_inc += include_directories('../../lib/librte_pci')
pmdinfogen = executable('pmdinfogen',
	'pmdinfogen.c',
	include_directories: pmdinfogen_inc,
	native: true)

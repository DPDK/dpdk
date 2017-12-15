# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

pmdinfogen_inc = eal_inc
pmdinfogen_inc += include_directories('../../lib/librte_pci')
pmdinfogen = executable('pmdinfogen',
	'pmdinfogen.c',
	include_directories: pmdinfogen_inc,
	native: true)

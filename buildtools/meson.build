# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2019 Intel Corporation

if host_machine.system() == 'windows'
	subdir_done()
endif

subdir('pmdinfogen')

pmdinfo = find_program('gen-pmdinfo-cfile.sh')

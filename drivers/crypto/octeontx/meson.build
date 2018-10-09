# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc
if host_machine.system() != 'linux'
	build = false
endif

deps += ['bus_pci']
name = 'octeontx_crypto'

sources = files('otx_cryptodev.c',
		'otx_cryptodev_ops.c')

cflags += '-DCPT_MODEL=CRYPTO_OCTEONTX'
includes += include_directories('../../common/cpt')

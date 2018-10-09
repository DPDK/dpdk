# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Cavium, Inc
if host_machine.system() != 'linux'
	build = false
endif

deps += ['bus_pci']
deps += ['common_cpt']
name = 'octeontx_crypto'

sources = files('otx_cryptodev.c',
		'otx_cryptodev_capabilities.c',
		'otx_cryptodev_hw_access.c',
		'otx_cryptodev_mbox.c',
		'otx_cryptodev_ops.c')

includes += include_directories('../../common/cpt')
cflags += '-DCPT_MODEL=CRYPTO_OCTEONTX'

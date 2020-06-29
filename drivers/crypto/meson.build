# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if is_windows
	subdir_done()
endif

drivers = ['aesni_gcm',
	   'aesni_mb',
	   'armv8',
	   'caam_jr',
	   'ccp',
	   'dpaa_sec',
	   'dpaa2_sec',
	   'kasumi',
	   'mvsam',
	   'nitrox',
	   'null',
	   'octeontx',
	   'octeontx2',
	   'openssl',
	   'qat',
	   'scheduler',
	   'snow3g',
	   'virtio',
	   'zuc']

std_deps = ['cryptodev'] # cryptodev pulls in all other needed deps
config_flag_fmt = 'RTE_LIBRTE_@0@_PMD'
driver_name_fmt = 'rte_pmd_@0@'

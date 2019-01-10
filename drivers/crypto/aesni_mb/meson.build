# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation
IPSec_MB_ver_0_52 = '0.52.0'
lib = cc.find_library('IPSec_MB', required: false)
if not lib.found()
	build = false
else
	ext_deps += lib

	imb_arr = cc.get_define('IMB_VERSION_STR',
		prefix : '#include<intel-ipsec-mb.h>').split('"')

	imb_ver =''.join(imb_arr)

	if imb_ver.version_compare('>=' + IPSec_MB_ver_0_52)
		message('Build for a new version of library IPSec_MB[' + imb_ver + ']')
		sources = files('rte_aesni_mb_pmd.c',
			'rte_aesni_mb_pmd_ops.c')
	else
		sources = files('rte_aesni_mb_pmd_compat.c',
			'rte_aesni_mb_pmd_ops_compat.c')
		message('Build for older version of library IPSec_MB[' + imb_ver + ']')
	endif

endif

allow_experimental_apis = true
deps += ['bus_vdev']

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

	imb_ver = ''.join(imb_arr)

	if (imb_ver == '') or (imb_ver.version_compare('<' + IPSec_MB_ver_0_52))
		message('IPSec_MB version >= 0.52 is required')
		build = false
	else
		sources = files('rte_aesni_mb_pmd.c', 'rte_aesni_mb_pmd_ops.c')
	endif

endif

allow_experimental_apis = true
deps += ['bus_vdev']

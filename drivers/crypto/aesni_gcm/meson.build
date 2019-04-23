# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

IMB_required_ver = '0.52.0'
lib = cc.find_library('IPSec_MB', required: false)
if not lib.found()
	build = false
else
	ext_deps += lib

	# version comes with quotes, so we split based on " and take the middle
	imb_ver = cc.get_define('IMB_VERSION_STR',
		prefix : '#include<intel-ipsec-mb.h>').split('"')[1]

	if (imb_ver == '') or (imb_ver.version_compare('<' + IMB_required_ver))
		message('IPSec_MB version >= @0@ is required, found version @1@'.format(
				IMB_required_ver, imb_ver))
		build = false
	endif
endif

allow_experimental_apis = true
sources = files('aesni_gcm_pmd.c', 'aesni_gcm_pmd_ops.c')
deps += ['bus_vdev']

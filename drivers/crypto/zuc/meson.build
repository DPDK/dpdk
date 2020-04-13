# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2020 Intel Corporation

IMB_required_ver = '0.53.0'
lib = cc.find_library('IPSec_MB', required: false)
if not lib.found()
	build = false
	reason = 'missing dependency, "libIPSec_MB"'
else
	# version comes with quotes, so we split based on " and take the middle
	imb_ver = cc.get_define('IMB_VERSION_STR',
		prefix : '#include<intel-ipsec-mb.h>').split('"')[1]

	if (imb_ver == '') or (imb_ver.version_compare('<' + IMB_required_ver))
		reason = 'IPSec_MB version >= @0@ is required, found version @1@'.format(
				IMB_required_ver, imb_ver)
		build = false
	endif

endif

ext_deps += lib
sources = files('rte_zuc_pmd.c', 'rte_zuc_pmd_ops.c')
deps += ['bus_vdev']

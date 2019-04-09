# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if not is_linux
        build = false
endif
sources = files('nfpcore/nfp_cpp_pcie_ops.c',
	'nfpcore/nfp_nsp.c',
	'nfpcore/nfp_cppcore.c',
	'nfpcore/nfp_resource.c',
	'nfpcore/nfp_mip.c',
	'nfpcore/nfp_nffw.c',
	'nfpcore/nfp_rtsym.c',
	'nfpcore/nfp_nsp_cmds.c',
	'nfpcore/nfp_crc.c',
	'nfpcore/nfp_mutex.c',
	'nfpcore/nfp_nsp_eth.c',
	'nfpcore/nfp_hwinfo.c',
	'nfp_net.c')

allow_experimental_apis = true

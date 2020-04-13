# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018-2019 Hisilicon Limited

if not is_linux
	build = false
	reason = 'only supported on Linux'
	subdir_done()
endif

if arch_subdir != 'x86' and arch_subdir != 'arm' or not dpdk_conf.get('RTE_ARCH_64')
	build = false
	reason = 'only supported on x86_64 and arm64'
	subdir_done()
endif

sources = files('hns3_cmd.c',
	'hns3_dcb.c',
	'hns3_intr.c',
	'hns3_ethdev.c',
	'hns3_ethdev_vf.c',
	'hns3_fdir.c',
	'hns3_flow.c',
	'hns3_mbx.c',
	'hns3_regs.c',
	'hns3_rss.c',
	'hns3_rxtx.c',
	'hns3_stats.c',
	'hns3_mp.c')

deps += ['hash']

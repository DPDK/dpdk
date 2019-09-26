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
	'hns3_ethdev.c',
	)
deps += ['hash']

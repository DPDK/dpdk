# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

eal_inc += include_directories('include')

env_objs = []
env_headers = files(
	'include/rte_os.h',
)
common_sources = files(
	'../../common/eal_common_errno.c',
	'../../common/eal_common_launch.c',
	'../../common/eal_common_lcore.c',
	'../../common/eal_common_log.c'
)
env_sources = files('eal.c',
	'eal_debug.c',
	'eal_lcore.c',
	'eal_thread.c',
)

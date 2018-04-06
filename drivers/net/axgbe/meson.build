# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.

if host_machine.system() != 'linux'
	build = false
endif

sources = files('axgbe_ethdev.c',
		'axgbe_dev.c',
		'axgbe_mdio.c',
		'axgbe_phy_impl.c',
		'axgbe_i2c.c',
		'axgbe_rxtx.c')

cflags += '-Wno-cast-qual'

if arch_subdir == 'x86'
	sources += files('axgbe_rxtx_vec_sse.c')
endif

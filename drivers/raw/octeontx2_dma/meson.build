# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2019 Marvell International Ltd.
#

deps += ['bus_pci', 'common_octeontx2', 'rawdev']
sources = files('otx2_dpi_rawdev.c', 'otx2_dpi_msg.c', 'otx2_dpi_test.c')

extra_flags = []
# This integrated controller runs only on a arm64 machine, remove 32bit warnings
if not dpdk_conf.get('RTE_ARCH_64')
	extra_flags += ['-Wno-int-to-pointer-cast', '-Wno-pointer-to-int-cast']
endif

foreach flag: extra_flags
	if cc.has_argument(flag)
		cflags += flag
	endif
endforeach

# Copyright (c) 2016-2018 Solarflare Communications Inc.
# All rights reserved.
#
# This software was jointly developed between OKTET Labs (under contract
# for Solarflare) and Solarflare Communications, Inc.

sources = [
	'efx_bootcfg.c',
	'efx_crc32.c',
	'efx_ev.c',
	'efx_filter.c',
	'efx_hash.c',
	'efx_intr.c',
	'efx_lic.c',
	'efx_mac.c',
	'efx_mcdi.c',
	'efx_mon.c',
	'efx_nic.c',
	'efx_nvram.c',
	'efx_phy.c',
	'efx_port.c',
	'efx_rx.c',
	'efx_sram.c',
	'efx_tunnel.c',
	'efx_tx.c',
	'efx_vpd.c',
	'mcdi_mon.c',
	'siena_mac.c',
	'siena_mcdi.c',
	'siena_nic.c',
	'siena_nvram.c',
	'siena_phy.c',
	'siena_sram.c',
	'siena_vpd.c',
	'ef10_ev.c',
	'ef10_filter.c',
	'ef10_image.c',
	'ef10_intr.c',
	'ef10_mac.c',
	'ef10_mcdi.c',
	'ef10_nic.c',
	'ef10_nvram.c',
	'ef10_phy.c',
	'ef10_rx.c',
	'ef10_tx.c',
	'ef10_vpd.c',
	'hunt_nic.c',
	'medford_nic.c',
	'medford2_nic.c'
]

extra_flags = [
	'-Wno-sign-compare',
	'-Wno-unused-parameter',
	'-Wno-unused-variable',
	'-Wno-empty-body',
	'-Wno-unused-but-set-variable'
]

c_args = cflags
foreach flag: extra_flags
	if cc.has_argument(flag)
		c_args += flag
	endif
endforeach

if build
	base_lib = static_library('sfc_base', sources,
		include_directories: includes,
		dependencies: static_rte_eal,
		c_args: c_args)

	base_objs = base_lib.extract_all_objects()
else
	base_objs = []
endif

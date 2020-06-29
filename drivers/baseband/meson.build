# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

if is_windows
	subdir_done()
endif

drivers = ['null', 'turbo_sw', 'fpga_lte_fec', 'fpga_5gnr_fec']

config_flag_fmt = 'RTE_LIBRTE_PMD_BBDEV_@0@'
driver_name_fmt = 'rte_pmd_bbdev_@0@'

# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sources = files('main.c',
		'test_bbdev.c',
		'test_bbdev_perf.c',
		'test_bbdev_vector.c')
deps += ['bbdev', 'bus_vdev']
if dpdk_conf.has('RTE_BASEBAND_FPGA_LTE_FEC')
	deps += ['baseband_fpga_lte_fec']
endif
if dpdk_conf.has('RTE_BASEBAND_FPGA_5GNR_FEC')
	deps += ['baseband_fpga_5gnr_fec']
endif
if dpdk_conf.has('RTE_BASEBAND_ACC100')
	deps += ['baseband_acc100']
endif

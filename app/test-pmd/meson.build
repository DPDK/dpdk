# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

sources = files('cmdline.c',
	'cmdline_flow.c',
	'cmdline_mtr.c',
	'cmdline_tm.c',
	'config.c',
	'csumonly.c',
	'flowgen.c',
	'icmpecho.c',
	'ieee1588fwd.c',
	'iofwd.c',
	'macfwd.c',
	'macswap.c',
	'parameters.c',
	'rxonly.c',
	'testpmd.c',
	'txonly.c')

deps = ['ethdev', 'gro', 'gso', 'cmdline', 'metrics', 'meter', 'bus_pci']
if dpdk_conf.has('RTE_LIBRTE_PDUMP')
	deps += 'pdump'
endif
if dpdk_conf.has('RTE_LIBRTE_I40E_PMD')
	deps += 'pmd_i40e'
endif
if dpdk_conf.has('RTE_LIBRTE_IXGBE_PMD')
	deps += 'pmd_ixgbe'
endif
if dpdk_conf.has('RTE_LIBRTE_SOFTNIC_PMD')
	sources += files('tm.c')
	deps += 'pmd_softnic'
endif

dep_objs = []
foreach d:deps
	dep_objs += get_variable(get_option('default_library') + '_rte_' + d)
endforeach
dep_objs += cc.find_library('execinfo', required: false) # for BSD only

link_libs = []
if get_option('default_library') == 'static'
	link_libs = dpdk_drivers
endif

executable('dpdk-testpmd',
	sources,
	c_args: [machine_args, '-DALLOW_EXPERIMENTAL_API'],
	link_whole: link_libs,
	dependencies: dep_objs,
	install_rpath: join_paths(get_option('prefix'), driver_install_path),
	install: true)

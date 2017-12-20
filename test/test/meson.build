# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

test_sources = files('commands.c',
	'packet_burst_generator.c',
	'test.c',
	'test_acl.c',
	'test_alarm.c',
	'test_atomic.c',
	'test_byteorder.c',
	'test_cmdline.c',
	'test_cmdline_cirbuf.c',
	'test_cmdline_etheraddr.c',
	'test_cmdline_ipaddr.c',
	'test_cmdline_lib.c',
	'test_cmdline_num.c',
	'test_cmdline_portlist.c',
	'test_cmdline_string.c',
	'test_common.c',
	'test_cpuflags.c',
	'test_crc.c',
	'test_cycles.c',
	'test_debug.c',
	'test_devargs.c',
	'test_distributor.c',
	'test_distributor_perf.c',
	'test_eal_flags.c',
	'test_eal_fs.c',
	'test_efd.c',
	'test_efd_perf.c',
	'test_errno.c',
	'test_event_ring.c',
	'test_eventdev.c',
	'test_func_reentrancy.c',
	'test_hash.c',
	'test_hash_functions.c',
	'test_hash_multiwriter.c',
	'test_hash_perf.c',
	'test_hash_scaling.c',
	'test_interrupts.c',
	'test_kni.c',
	'test_kvargs.c',
	'test_logs.c',
	'test_lpm.c',
	'test_lpm6.c',
	'test_lpm6_perf.c',
	'test_lpm_perf.c',
	'test_malloc.c',
	'test_mbuf.c',
	'test_memcpy.c',
	'test_memcpy_perf.c',
	'test_memory.c',
	'test_mempool.c',
	'test_mempool_perf.c',
	'test_memzone.c',
	'test_meter.c',
	'test_mp_secondary.c',
	'test_per_lcore.c',
	'test_pmd_perf.c',
	'test_power.c',
	'test_power_acpi_cpufreq.c',
	'test_power_kvm_vm.c',
	'test_prefetch.c',
	'test_red.c',
	'test_reorder.c',
	'test_ring.c',
	'test_ring_perf.c',
	'test_rwlock.c',
	'test_sched.c',
	'test_service_cores.c',
	'test_spinlock.c',
	'test_string_fns.c',
	'test_table.c',
	'test_table_acl.c',
	'test_table_combined.c',
	'test_table_pipeline.c',
	'test_table_ports.c',
	'test_table_tables.c',
	'test_tailq.c',
	'test_thash.c',
	'test_timer.c',
	'test_timer_perf.c',
	'test_timer_racecond.c',
	'test_version.c',
	'virtual_pmd.c'
)

test_deps = ['acl',
	'cfgfile',
	'cmdline',
	'cryptodev',
	'distributor',
	'efd',
	'ethdev',
	'eventdev',
	'hash',
	'lpm',
	'pipeline',
	'port',
	'power',
	'reorder',
	'ring',
	'timer'
]

test_names = [
	'acl_autotest',
	'alarm_autotest',
	'atomic_autotest',
	'byteorder_autotest',
	'cmdline_autotest',
	'common_autotest',
	'cpuflags_autotest',
	'crc_autotest',
	'cycles_autotest',
	'debug_autotest',
	'devargs_autotest',
	'distributor_autotest',
	'distributor_perf_autotest',
	'eal_flags_autotest',
	'eal_fs_autotest',
	'efd_autotest',
	'efd_perf_autotest',
	'errno_autotest',
	'event_ring_autotest',
	'eventdev_common_autotest',
	'eventdev_octeontx_autotest',
	'eventdev_sw_autotest',
	'func_reentrancy_autotest',
	'hash_scaling_autotest',
	'hash_autotest',
	'hash_functions_autotest',
	'hash_multiwriter_autotest',
	'hash_perf_autotest',
	'interrupt_autotest',
	'kni_autotest',
	'kvargs_autotest',
	'logs_autotest',
	'lpm6_autotest',
	'lpm6_perf_autotest',
	'lpm_autotest',
	'lpm_perf_autotest',
	'malloc_autotest',
	'mbuf_autotest',
	'memcpy_autotest',
	'memcpy_perf_autotest',
	'memory_autotest',
	'mempool_autotest',
	'mempool_perf_autotest',
	'memzone_autotest',
	'meter_autotest',
	'multiprocess_autotest',
	'per_lcore_autotest',
	'pmd_perf_autotest',
	'power_acpi_cpufreq_autotest',
	'power_autotest',
	'power_kvm_vm_autotest',
	'prefetch_autotest',
	'red_all',
	'red_autotest',
	'red_perf',
	'reorder_autotest',
	'ring_autotest',
	'ring_perf_autotest',
	'rwlock_autotest',
	'sched_autotest',
	'service_autotest',
	'spinlock_autotest',
	'table_autotest',
	'tailq_autotest',
	'thash_autotest',
	'timer_autotest',
	'timer_perf__autotest',
	'timer_racecond_autotest',
	'user_delay_us',
	'version_autotest',
]

if dpdk_conf.has('RTE_LIBRTE_PDUMP')
	test_deps += 'pdump'
endif
if dpdk_conf.has('RTE_LIBRTE_I40E_PMD')
	test_deps += 'pmd_i40e'
endif
if dpdk_conf.has('RTE_LIBRTE_IXGBE_PMD')
	test_deps += 'pmd_ixgbe'
endif

test_dep_objs = []
foreach d:test_deps
	def_lib = get_option('default_library')
	test_dep_objs += get_variable(def_lib + '_rte_' + d)
endforeach

link_libs = []
if get_option('default_library') == 'static'
	link_libs = dpdk_drivers
endif

if get_option('tests')
	dpdk_test = executable('dpdk-test',
		test_sources,
		link_whole: link_libs,
		dependencies: test_dep_objs,
		c_args: [machine_arg, '-DALLOW_EXPERIMENTAL_API'],
		install_rpath: driver_install_path,
		install: false)

	# some perf tests (eg: memcpy perf autotest)take very long
	# to complete, so timeout to 10 minutes
	timeout_seconds = 600

	foreach t:test_names
		test(t, dpdk_test,
			env : ['DPDK_TEST='+t],
			timeout : timeout_seconds,
			is_parallel : false)
	endforeach
endif

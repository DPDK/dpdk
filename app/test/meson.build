# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

test_sources = files('commands.c',
	'packet_burst_generator.c',
	'sample_packet_forward.c',
	'test.c',
	'test_acl.c',
	'test_alarm.c',
	'test_atomic.c',
	'test_barrier.c',
	'test_bitratestats.c',
	'test_bpf.c',
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
	'test_cryptodev.c',
	'test_cryptodev_asym.c',
	'test_cryptodev_blockcipher.c',
	'test_cycles.c',
	'test_debug.c',
	'test_distributor.c',
	'test_distributor_perf.c',
	'test_eal_flags.c',
	'test_eal_fs.c',
	'test_efd.c',
	'test_efd_perf.c',
	'test_errno.c',
	'test_event_crypto_adapter.c',
	'test_event_eth_rx_adapter.c',
	'test_event_ring.c',
	'test_event_eth_tx_adapter.c',
	'test_event_timer_adapter.c',
	'test_eventdev.c',
	'test_external_mem.c',
	'test_fbarray.c',
	'test_func_reentrancy.c',
	'test_flow_classify.c',
	'test_hash.c',
	'test_hash_functions.c',
	'test_hash_multiwriter.c',
	'test_hash_readwrite.c',
	'test_hash_perf.c',
	'test_hash_readwrite_lf.c',
	'test_interrupts.c',
	'test_ipsec.c',
	'test_kni.c',
	'test_kvargs.c',
	'test_latencystats.c',
	'test_link_bonding.c',
	'test_link_bonding_mode4.c',
	'test_logs.c',
	'test_lpm.c',
	'test_lpm6.c',
	'test_lpm6_perf.c',
	'test_lpm_perf.c',
	'test_malloc.c',
	'test_mbuf.c',
	'test_member.c',
	'test_member_perf.c',
	'test_memcpy.c',
	'test_memcpy_perf.c',
	'test_memory.c',
	'test_mempool.c',
	'test_mempool_perf.c',
	'test_memzone.c',
	'test_meter.c',
	'test_metrics.c',
	'test_mp_secondary.c',
	'test_pdump.c',
	'test_per_lcore.c',
	'test_pmd_perf.c',
	'test_pmd_ring.c',
	'test_pmd_ring_perf.c',
	'test_power.c',
	'test_power_acpi_cpufreq.c',
	'test_power_kvm_vm.c',
	'test_prefetch.c',
	'test_rcu_qsbr.c',
	'test_rcu_qsbr_perf.c',
	'test_reciprocal_division.c',
	'test_reciprocal_division_perf.c',
	'test_red.c',
	'test_reorder.c',
	'test_ring.c',
	'test_ring_perf.c',
	'test_rwlock.c',
	'test_sched.c',
	'test_service_cores.c',
	'test_spinlock.c',
	'test_stack.c',
	'test_stack_perf.c',
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
	'test_timer_secondary.c',
	'test_ticketlock.c',
	'test_version.c',
	'virtual_pmd.c'
)

test_deps = ['acl',
	'bitratestats',
	'bpf',
	'cfgfile',
	'cmdline',
	'cryptodev',
	'distributor',
	'efd',
	'ethdev',
	'eventdev',
	'flow_classify',
	'hash',
	'ipsec',
	'latencystats',
	'lpm',
	'member',
	'metrics',
	'pipeline',
	'port',
	'rcu',
	'reorder',
	'ring',
	'stack',
	'timer'
]

# All test cases in fast_parallel_test_names list are parallel
fast_parallel_test_names = [
        'acl_autotest',
        'alarm_autotest',
        'atomic_autotest',
        'byteorder_autotest',
        'cmdline_autotest',
        'common_autotest',
        'cpuflags_autotest',
        'cycles_autotest',
        'debug_autotest',
        'eal_flags_autotest',
        'eal_fs_autotest',
        'errno_autotest',
        'event_ring_autotest',
        'func_reentrancy_autotest',
        'flow_classify_autotest',
        'hash_autotest',
        'interrupt_autotest',
        'logs_autotest',
        'lpm_autotest',
        'lpm6_autotest',
        'malloc_autotest',
        'mbuf_autotest',
        'memcpy_autotest',
        'memory_autotest',
        'mempool_autotest',
        'memzone_autotest',
        'meter_autotest',
        'multiprocess_autotest',
        'per_lcore_autotest',
        'prefetch_autotest',
        'rcu_qsbr_autotest',
        'red_autotest',
        'ring_autotest',
        'ring_pmd_autotest',
        'rwlock_autotest',
        'sched_autotest',
        'spinlock_autotest',
        'stack_autotest',
        'stack_nb_autotest',
        'string_autotest',
        'table_autotest',
        'tailq_autotest',
        'timer_autotest',
        'user_delay_us',
        'version_autotest',
]

# All test cases in fast_non_parallel_test_names list are non-parallel
fast_non_parallel_test_names = [
        'bitratestats_autotest',
        'cryptodev_sw_armv8_autotest',
        'crc_autotest',
        'cryptodev_openssl_asym_autotest',
        'cryptodev_sw_mvsam_autotest',
        'delay_us_sleep_autotest',
        'distributor_autotest',
        'eventdev_common_autotest',
        'eventdev_octeontx_autotest',
        'eventdev_sw_autotest',
        'fbarray_autotest',
        'hash_readwrite_autotest',
        'hash_readwrite_lf_autotest',
        'hash_scaling_autotest',
        'ipsec_autotest',
        'kni_autotest',
        'kvargs_autotest',
        'latencystats_autotest',
        'member_autotest',
        'metrics_autotest',
        'pdump_autotest',
        'power_acpi_cpufreq_autotest',
        'power_autotest',
        'power_kvm_vm_autotest',
        'reorder_autotest',
        'service_autotest',
        'thash_autotest',
]

# All test cases in perf_test_names list are non-parallel
perf_test_names = [
        'ring_perf_autotest',
        'mempool_perf_autotest',
        'memcpy_perf_autotest',
        'hash_perf_autotest',
        'timer_perf_autotest',
        'reciprocal_division',
        'reciprocal_division_perf',
        'lpm_perf_autotest',
        'red_all',
        'barrier_autotest',
        'hash_multiwriter_autotest',
        'timer_racecond_autotest',
        'efd_autotest',
        'hash_functions_autotest',
        'eventdev_selftest_sw',
        'member_perf_autotest',
        'efd_perf_autotest',
        'lpm6_perf_autotest',
        'rcu_qsbr_perf_autotest',
        'red_perf',
        'distributor_perf_autotest',
        'ring_pmd_perf_autotest',
        'pmd_perf_autotest',
        'stack_perf_autotest',
        'stack_nb_perf_autotest',
]

# All test cases in driver_test_names list are non-parallel
driver_test_names = [
        'link_bonding_autotest',
        'link_bonding_mode4_autotest',
        'link_bonding_rssconf_autotest',
        'cryptodev_sw_mrvl_autotest',
        'cryptodev_dpaa2_sec_autotest',
        'cryptodev_dpaa_sec_autotest',
        'cryptodev_qat_autotest',
        'cryptodev_aesni_mb_autotest',
        'cryptodev_openssl_autotest',
        'cryptodev_scheduler_autotest',
        'cryptodev_aesni_gcm_autotest',
        'cryptodev_null_autotest',
        'cryptodev_sw_snow3g_autotest',
        'cryptodev_sw_kasumi_autotest',
        'cryptodev_sw_zuc_autotest',
]

# All test cases in dump_test_names list are non-parallel
dump_test_names = [
        'dump_struct_sizes',
        'dump_mempool',
        'dump_malloc_stats',
        'dump_devargs',
        'dump_log_types',
        'dump_ring',
        'dump_physmem',
        'dump_memzone',
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
if dpdk_conf.has('RTE_LIBRTE_BOND_PMD')
	test_deps += 'pmd_bond'
endif
if dpdk_conf.has('RTE_LIBRTE_RING_PMD')
	test_deps += 'pmd_ring'
endif
if dpdk_conf.has('RTE_LIBRTE_POWER')
	test_deps += 'power'
endif
if dpdk_conf.has('RTE_LIBRTE_KNI')
	test_deps += 'kni'
endif

cflags = machine_args
if cc.has_argument('-Wno-format-truncation')
    cflags += '-Wno-format-truncation'
endif

# specify -D_GNU_SOURCE unconditionally
cflags += '-D_GNU_SOURCE'

test_dep_objs = []
if dpdk_conf.has('RTE_LIBRTE_COMPRESSDEV')
	compress_test_dep = dependency('zlib', required: false)
	if compress_test_dep.found()
		test_dep_objs += compress_test_dep
		test_sources += 'test_compressdev.c'
		test_deps += 'compressdev'
		fast_non_parallel_test_names += 'compressdev_autotest'
	endif
endif

foreach d:test_deps
	def_lib = get_option('default_library')
	test_dep_objs += get_variable(def_lib + '_rte_' + d)
endforeach
test_dep_objs += cc.find_library('execinfo', required: false)

link_libs = []
if get_option('default_library') == 'static'
	link_libs = dpdk_drivers
endif

if get_option('tests')
	dpdk_test = executable('dpdk-test',
		test_sources,
		link_whole: link_libs,
		dependencies: test_dep_objs,
		c_args: [cflags, '-DALLOW_EXPERIMENTAL_API'],
		install_rpath: driver_install_path,
		install: false)

	# some perf tests (eg: memcpy perf autotest)take very long
	# to complete, so timeout to 10 minutes
	timeout_seconds = 600
	timeout_seconds_fast = 10

	# Retrieve the number of CPU cores, defaulting to 4.
	num_cores = '0-3'
	if host_machine.system() == 'linux'
		num_cores = run_command('cat',
					'/sys/devices/system/cpu/present'
				       ).stdout().strip()
	elif host_machine.system() == 'freebsd'
		snum_cores = run_command('/sbin/sysctl', '-n',
					 'hw.ncpu').stdout().strip()
		inum_cores = snum_cores.to_int() - 1
                num_cores = '0-@0@'.format(inum_cores)
	endif

	num_cores_arg = '-l ' + num_cores

	test_args = [num_cores_arg, '-n 4']
	foreach arg : fast_parallel_test_names
		if host_machine.system() == 'linux'
			test(arg, dpdk_test,
				  env : ['DPDK_TEST=' + arg],
				  args : test_args +
					 ['--file-prefix=@0@'.format(arg)],
			timeout : timeout_seconds_fast,
			suite : 'fast-tests')
		else
			test(arg, dpdk_test,
				env : ['DPDK_TEST=' + arg],
				args : test_args,
			timeout : timeout_seconds_fast,
			suite : 'fast-tests')
		endif
	endforeach

	foreach arg : fast_non_parallel_test_names
		test(arg, dpdk_test,
			env : ['DPDK_TEST=' + arg],
			args : test_args,
			timeout : timeout_seconds_fast,
			is_parallel : false,
			suite : 'fast-tests')
	endforeach

	foreach arg : perf_test_names
		test(arg, dpdk_test,
		env : ['DPDK_TEST=' + arg],
		args : test_args,
		timeout : timeout_seconds,
		is_parallel : false,
		suite : 'perf-tests')
	endforeach

	foreach arg : driver_test_names
		test(arg, dpdk_test,
			env : ['DPDK_TEST=' + arg],
			args : test_args,
			timeout : timeout_seconds,
			is_parallel : false,
			suite : 'driver-tests')
	endforeach

	foreach arg : dump_test_names
		test(arg, dpdk_test,
			env : ['DPDK_TEST=' + arg],
			args : test_args,
			timeout : timeout_seconds,
			is_parallel : false,
			suite : 'debug-tests')
	endforeach
endif

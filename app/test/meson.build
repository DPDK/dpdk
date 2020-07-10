# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

if not get_option('tests')
	subdir_done()
endif

test_sources = files('commands.c',
	'packet_burst_generator.c',
	'test.c',
	'test_acl.c',
	'test_alarm.c',
	'test_atomic.c',
	'test_barrier.c',
	'test_bitops.c',
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
	'test_cryptodev_security_pdcp.c',
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
	'test_event_timer_adapter.c',
	'test_eventdev.c',
	'test_external_mem.c',
	'test_fbarray.c',
	'test_fib.c',
	'test_fib_perf.c',
	'test_fib6.c',
	'test_fib6_perf.c',
	'test_func_reentrancy.c',
	'test_flow_classify.c',
	'test_graph.c',
	'test_graph_perf.c',
	'test_hash.c',
	'test_hash_functions.c',
	'test_hash_multiwriter.c',
	'test_hash_readwrite.c',
	'test_hash_perf.c',
	'test_hash_readwrite_lf_perf.c',
	'test_interrupts.c',
        'test_ipfrag.c',
	'test_ipsec.c',
	'test_ipsec_sad.c',
	'test_ipsec_perf.c',
	'test_kni.c',
	'test_kvargs.c',
	'test_lcores.c',
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
	'test_mcslock.c',
	'test_mp_secondary.c',
	'test_per_lcore.c',
	'test_pmd_perf.c',
	'test_power.c',
	'test_power_cpufreq.c',
	'test_power_kvm_vm.c',
	'test_prefetch.c',
	'test_rand_perf.c',
	'test_rawdev.c',
	'test_rcu_qsbr.c',
	'test_rcu_qsbr_perf.c',
	'test_reciprocal_division.c',
	'test_reciprocal_division_perf.c',
	'test_red.c',
	'test_reorder.c',
	'test_rib.c',
	'test_rib6.c',
	'test_ring.c',
	'test_ring_mpmc_stress.c',
	'test_ring_hts_stress.c',
	'test_ring_mt_peek_stress.c',
	'test_ring_perf.c',
	'test_ring_rts_stress.c',
	'test_ring_st_peek_stress.c',
	'test_ring_stress.c',
	'test_rwlock.c',
	'test_sched.c',
	'test_security.c',
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
	'test_trace.c',
	'test_trace_register.c',
	'test_trace_perf.c',
	'test_version.c',
	'virtual_pmd.c'
)

test_deps = ['acl',
	'bus_pci',
	'bus_vdev',
	'bitratestats',
	'bpf',
	'cfgfile',
	'cmdline',
	'cryptodev',
	'distributor',
	'efd',
	'ethdev',
	'eventdev',
	'fib',
	'flow_classify',
	'graph',
	'hash',
	'ipsec',
	'latencystats',
	'lpm',
	'member',
	'metrics',
	'node',
	'pipeline',
	'port',
	'rawdev',
	'rcu',
	'reorder',
	'rib',
	'ring',
	'security',
	'stack',
	'timer'
]

# Each test is marked with flag true/false
# to indicate whether it can run in no-huge mode.
fast_tests = [
        ['acl_autotest', true],
        ['alarm_autotest', false],
        ['atomic_autotest', false],
        ['bitops_autotest', true],
        ['byteorder_autotest', true],
        ['cmdline_autotest', true],
        ['common_autotest', true],
        ['cpuflags_autotest', true],
        ['cycles_autotest', true],
        ['debug_autotest', true],
        ['eal_flags_c_opt_autotest', false],
        ['eal_flags_master_opt_autotest', false],
        ['eal_flags_n_opt_autotest', false],
        ['eal_flags_hpet_autotest', false],
        ['eal_flags_no_huge_autotest', false],
        ['eal_flags_w_opt_autotest', false],
        ['eal_flags_b_opt_autotest', false],
        ['eal_flags_vdev_opt_autotest', false],
        ['eal_flags_r_opt_autotest', false],
        ['eal_flags_mem_autotest', false],
        ['eal_flags_file_prefix_autotest', false],
        ['eal_flags_misc_autotest', false],
        ['eal_fs_autotest', true],
        ['errno_autotest', true],
        ['event_ring_autotest', true],
        ['fib_autotest', true],
        ['fib6_autotest', true],
        ['func_reentrancy_autotest', false],
        ['flow_classify_autotest', false],
        ['hash_autotest', true],
        ['interrupt_autotest', true],
        ['ipfrag_autotest', false],
        ['lcores_autotest', true],
        ['logs_autotest', true],
        ['lpm_autotest', true],
        ['lpm6_autotest', true],
        ['malloc_autotest', false],
        ['mbuf_autotest', false],
        ['mcslock_autotest', false],
        ['memcpy_autotest', true],
        ['memory_autotest', false],
        ['mempool_autotest', false],
        ['memzone_autotest', false],
        ['meter_autotest', true],
        ['multiprocess_autotest', false],
        ['per_lcore_autotest', true],
        ['prefetch_autotest', true],
        ['rcu_qsbr_autotest', true],
        ['red_autotest', true],
        ['rib_autotest', true],
        ['rib6_autotest', true],
        ['ring_autotest', true],
        ['rwlock_test1_autotest', true],
        ['rwlock_rda_autotest', true],
        ['rwlock_rds_wrm_autotest', true],
        ['rwlock_rde_wro_autotest', true],
        ['sched_autotest', true],
		['security_autotest', false],
        ['spinlock_autotest', true],
        ['stack_autotest', false],
        ['stack_lf_autotest', false],
        ['string_autotest', true],
        ['table_autotest', true],
        ['tailq_autotest', true],
        ['timer_autotest', false],
        ['user_delay_us', true],
        ['version_autotest', true],
        ['crc_autotest', true],
        ['delay_us_sleep_autotest', true],
        ['distributor_autotest', false],
        ['eventdev_common_autotest', true],
        ['fbarray_autotest', true],
        ['hash_readwrite_func_autotest', false],
        ['ipsec_autotest', true],
        ['kni_autotest', false],
        ['kvargs_autotest', true],
        ['member_autotest', true],
        ['metrics_autotest', true],
        ['power_cpufreq_autotest', false],
        ['power_autotest', true],
        ['power_kvm_vm_autotest', false],
        ['reorder_autotest', true],
        ['service_autotest', true],
        ['thash_autotest', true],
        ['trace_autotest', true],
]

perf_test_names = [
        'ring_perf_autotest',
        'mempool_perf_autotest',
        'memcpy_perf_autotest',
        'hash_perf_autotest',
        'timer_perf_autotest',
        'reciprocal_division',
        'reciprocal_division_perf',
        'lpm_perf_autotest',
        'rib_slow_autotest',
        'fib_slow_autotest',
        'fib_perf_autotest',
        'red_all',
        'barrier_autotest',
        'hash_multiwriter_autotest',
        'timer_racecond_autotest',
        'efd_autotest',
        'hash_functions_autotest',
        'member_perf_autotest',
        'efd_perf_autotest',
        'lpm6_perf_autotest',
        'rib6_slow_autotest',
        'fib6_slow_autotest',
        'fib6_perf_autotest',
        'rcu_qsbr_perf_autotest',
        'red_perf',
        'distributor_perf_autotest',
        'pmd_perf_autotest',
        'stack_perf_autotest',
        'stack_lf_perf_autotest',
        'rand_perf_autotest',
        'hash_readwrite_perf_autotest',
        'hash_readwrite_lf_perf_autotest',
        'trace_perf_autotest',
	'ipsec_perf_autotest',
]

driver_test_names = [
        'cryptodev_aesni_mb_autotest',
        'cryptodev_aesni_gcm_autotest',
        'cryptodev_dpaa_sec_autotest',
        'cryptodev_dpaa2_sec_autotest',
        'cryptodev_null_autotest',
        'cryptodev_octeontx2_autotest',
        'cryptodev_openssl_autotest',
        'cryptodev_openssl_asym_autotest',
        'cryptodev_qat_autotest',
        'cryptodev_sw_armv8_autotest',
        'cryptodev_sw_kasumi_autotest',
        'cryptodev_sw_mvsam_autotest',
        'cryptodev_sw_snow3g_autotest',
        'cryptodev_sw_zuc_autotest',
        'eventdev_selftest_octeontx',
        'eventdev_selftest_sw',
        'rawdev_autotest',
]

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

# The following linkages are an exception to allow running the
# unit tests without requiring that the developer install the
# DPDK libraries.  Explicit linkage of drivers (plugin libraries)
# in applications should not be used.
if dpdk_conf.has('RTE_LIBRTE_RING_MEMPOOL')
	test_deps += 'mempool_ring'
endif
if dpdk_conf.has('RTE_LIBRTE_STACK_MEMPOOL')
	test_deps += 'mempool_stack'
endif
if dpdk_conf.has('RTE_LIBRTE_SKELETON_EVENTDEV_PMD')
	test_deps += 'pmd_skeleton_event'
endif
if dpdk_conf.has('RTE_LIBRTE_TELEMETRY')
	test_sources += 'test_telemetry_json.c'
	fast_tests += [['telemetry_json_autotest', true]]
endif

# The following linkages of drivers are required because
# they are used via a driver-specific API.
if dpdk_conf.has('RTE_LIBRTE_BOND_PMD')
	test_deps += 'pmd_bond'
	test_sources += ['test_link_bonding.c', 'test_link_bonding_rssconf.c']
	driver_test_names += ['link_bonding_autotest', 'link_bonding_rssconf_autotest']
	if dpdk_conf.has('RTE_LIBRTE_RING_PMD')
		test_sources += 'test_link_bonding_mode4.c'
		driver_test_names += 'link_bonding_mode4_autotest'
	endif
endif
if dpdk_conf.has('RTE_LIBRTE_RING_PMD')
	test_deps += 'pmd_ring'
	test_sources += 'test_pmd_ring_perf.c'
	test_sources += 'test_pmd_ring.c'
	test_sources += 'test_event_eth_tx_adapter.c'
	test_sources += 'test_bitratestats.c'
	test_sources += 'test_latencystats.c'
	test_sources += 'sample_packet_forward.c'
	test_sources += 'test_pdump.c'
	fast_tests += [['ring_pmd_autotest', true]]
	perf_test_names += 'ring_pmd_perf_autotest'
	fast_tests += [['event_eth_tx_adapter_autotest', false]]
	fast_tests += [['bitratestats_autotest', true]]
	fast_tests += [['latencystats_autotest', true]]
	fast_tests += [['pdump_autotest', true]]
endif

if dpdk_conf.has('RTE_LIBRTE_POWER')
	test_deps += 'power'
endif
if dpdk_conf.has('RTE_LIBRTE_KNI')
	test_deps += 'kni'
endif
if dpdk_conf.has('RTE_LIBRTE_PDUMP')
	test_deps += 'pdump'
endif

if cc.has_argument('-Wno-format-truncation')
    cflags += '-Wno-format-truncation'
endif

# specify -D_GNU_SOURCE unconditionally
cflags += '-D_GNU_SOURCE'
# Strict-aliasing rules are violated by uint8_t[] to context size casts.
cflags += '-fno-strict-aliasing'

# Enable using internal APIs in unit tests
cflags += ['-DALLOW_INTERNAL_API']

test_dep_objs = []
if dpdk_conf.has('RTE_LIBRTE_COMPRESSDEV')
	compress_test_dep = dependency('zlib', required: false)
	if compress_test_dep.found()
		test_dep_objs += compress_test_dep
		test_sources += 'test_compressdev.c'
		test_deps += 'compressdev'
		fast_tests += [['compressdev_autotest', false]]
	endif
endif

if dpdk_conf.has('RTE_LIBRTE_PMD_CRYPTO_SCHEDULER')
	driver_test_names += 'cryptodev_scheduler_autotest'
endif

foreach d:test_deps
	def_lib = get_option('default_library')
	test_dep_objs += get_variable(def_lib + '_rte_' + d)
endforeach
test_dep_objs += cc.find_library('execinfo', required: false)

link_libs = []
if get_option('default_library') == 'static'
	link_libs = dpdk_static_libraries + dpdk_drivers
endif

dpdk_test = executable('dpdk-test',
	test_sources,
	link_whole: link_libs,
	dependencies: test_dep_objs,
	c_args: cflags,
	install_rpath: join_paths(get_option('prefix'),
			 driver_install_path),
	install: true)

has_hugepage = run_command('has-hugepage.sh').stdout().strip() != '0'
message('hugepage availability: @0@'.format(has_hugepage))

# some perf tests (eg: memcpy perf autotest)take very long
# to complete, so timeout to 10 minutes
timeout_seconds = 600
timeout_seconds_fast = 10

get_coremask = find_program('get-coremask.sh')
num_cores_arg = '-l ' + run_command(get_coremask).stdout().strip()

default_test_args = [num_cores_arg]

foreach arg : fast_tests
	test_args = default_test_args
	run_test = true
	if not has_hugepage
		if arg[1]
			test_args += ['--no-huge', '-m', '2048']
		else
			run_test = false
		endif
	endif

	if (get_option('default_library') == 'shared' and
		arg[0] == 'event_eth_tx_adapter_autotest')
		foreach drv:dpdk_drivers
			test_args += ['-d', drv.full_path().split('.a')[0] + '.so']
		endforeach
	endif
	if is_linux
		test_args += ['--file-prefix=@0@'.format(arg[0])]
	endif

	if run_test
		test(arg[0], dpdk_test,
			env : ['DPDK_TEST=' + arg[0]],
			args : test_args,
			timeout : timeout_seconds_fast,
			is_parallel : false,
			suite : 'fast-tests')
	endif
endforeach

foreach arg : perf_test_names
	test(arg, dpdk_test,
	env : ['DPDK_TEST=' + arg],
	args : default_test_args,
	timeout : timeout_seconds,
	is_parallel : false,
	suite : 'perf-tests')
endforeach

foreach arg : driver_test_names
	test(arg, dpdk_test,
		env : ['DPDK_TEST=' + arg],
		args : default_test_args,
		timeout : timeout_seconds,
		is_parallel : false,
		suite : 'driver-tests')
endforeach

foreach arg : dump_test_names
	test(arg, dpdk_test,
		env : ['DPDK_TEST=' + arg],
		args : default_test_args,
		timeout : timeout_seconds,
		is_parallel : false,
		suite : 'debug-tests')
endforeach
